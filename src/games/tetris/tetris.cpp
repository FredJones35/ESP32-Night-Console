// BLOKLAR — düşen blok bulmacası: 10x20 alan, sıradaki parça önizlemesi,
// satır temizleme efekti, hızlanan seviyeler, hayalet parça, beklet (hold).
#include <math.h>
#include <new>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../core/game.h"
#include "../../core/gfx.h"
#include "../../core/hw.h"
#include "../../core/input.h"
#include "../../core/save.h"
#include "../../core/ui.h"

namespace tetris {

static const int W = 10, H = 20, CELL = 6;
static const int FX = 4, FY = 4;   // alanın ekrandaki yeri

// 7 parça x 4 dönüş, her biri 4x4 bit maskesi (bit 15 = sol üst)
static const uint16_t SHAPES[7][4] = {
  { 0x0F00, 0x2222, 0x00F0, 0x4444 },   // I
  { 0x6600, 0x6600, 0x6600, 0x6600 },   // O
  { 0x4E00, 0x4640, 0x0E40, 0x4C40 },   // T
  { 0x6C00, 0x4620, 0x06C0, 0x8C40 },   // S
  { 0xC600, 0x2640, 0x0C60, 0x4C80 },   // Z
  { 0x8E00, 0x6440, 0x0E20, 0x44C0 },   // J
  { 0x2E00, 0x4460, 0x0E80, 0xC440 },   // L
};
static const uint16_t COLORS[8] = {
  0, rgb(0, 220, 230), rgb(240, 220, 0), rgb(180, 60, 230), rgb(60, 220, 70),
  rgb(235, 50, 50), rgb(50, 100, 240), rgb(245, 150, 20),
};

enum State : uint8_t { READY, PLAY, CLEARING, OVER };

struct World {
  uint8_t field[H][W];   // 0 boş, 1..7 renk
  int piece, rot, px, py, next, hold;
  bool holdUsed;
  uint8_t bag[7];
  int bagN;
  int score, lines, level, best;
  float fallT, lockT, clearT, t, overT;
  uint8_t clearRows[4];
  int nClear;
  State st;
  bool newBest;
};
static World* w = nullptr;

static inline bool cellOf(uint16_t m, int x, int y) { return m & (0x8000 >> (y * 4 + x)); }

static bool fits(int piece, int rot, int px, int py) {
  uint16_t m = SHAPES[piece][rot & 3];
  for (int y = 0; y < 4; y++)
    for (int x = 0; x < 4; x++) {
      if (!cellOf(m, x, y)) continue;
      int fx = px + x, fy = py + y;
      if (fx < 0 || fx >= W || fy >= H) return false;
      if (fy >= 0 && w->field[fy][fx]) return false;
    }
  return true;
}

static int fromBag() {
  if (w->bagN == 0) {
    for (int i = 0; i < 7; i++) w->bag[i] = (uint8_t)i;
    for (int i = 6; i > 0; i--) {
      int j = (int)(hwRandom() % (uint32_t)(i + 1));
      uint8_t t = w->bag[i]; w->bag[i] = w->bag[j]; w->bag[j] = t;
    }
    w->bagN = 7;
  }
  return w->bag[--w->bagN];
}

static float gravity() {
  float g = 0.8f * powf(0.84f, (float)(w->level - 1));
  return g < 0.04f ? 0.04f : g;
}

static void spawn(int piece) {
  w->piece = piece;
  w->rot = 0;
  w->px = 3;
  w->py = -1;
  w->fallT = 0;
  w->lockT = 0;
  if (!fits(w->piece, w->rot, w->px, w->py)) {
    w->st = OVER;
    w->overT = 0;
    if (w->score > 0 && save::setMax("tet_hi", w->score)) { w->best = w->score; w->newBest = true; }
    input::clear();
  }
}

static void reset() {
  memset(w->field, 0, sizeof(w->field));
  w->bagN = 0;
  w->score = w->lines = 0;
  w->level = 1;
  w->hold = -1;
  w->holdUsed = false;
  w->newBest = false;
  w->next = fromBag();
  spawn(fromBag());
}

static void lockPiece() {
  uint16_t m = SHAPES[w->piece][w->rot];
  for (int y = 0; y < 4; y++)
    for (int x = 0; x < 4; x++)
      if (cellOf(m, x, y) && w->py + y >= 0) w->field[w->py + y][w->px + x] = (uint8_t)(w->piece + 1);
  w->nClear = 0;
  for (int y = 0; y < H; y++) {
    bool full = true;
    for (int x = 0; x < W && full; x++) full = w->field[y][x] != 0;
    if (full && w->nClear < 4) w->clearRows[w->nClear++] = (uint8_t)y;
  }
  w->holdUsed = false;
  if (w->nClear) {
    static const int PTS[5] = { 0, 100, 300, 500, 800 };
    w->score += PTS[w->nClear] * w->level;
    w->lines += w->nClear;
    w->level = 1 + w->lines / 10;
    w->st = CLEARING;
    w->clearT = 0.3f;
    return;
  }
  int n = w->next;
  w->next = fromBag();
  spawn(n);
}

static void finishClear() {
  for (int i = 0; i < w->nClear; i++) {
    int row = w->clearRows[i];
    for (int y = row; y > 0; y--) memcpy(w->field[y], w->field[y - 1], W);
    memset(w->field[0], 0, W);
  }
  w->nClear = 0;
  w->st = PLAY;
  int n = w->next;
  w->next = fromBag();
  spawn(n);
}

static void rotate() {
  int nr = (w->rot + 1) & 3;
  static const int8_t KICKS[][2] = { {0, 0}, {-1, 0}, {1, 0}, {-2, 0}, {2, 0}, {0, -1} };
  for (auto& k : KICKS)
    if (fits(w->piece, nr, w->px + k[0], w->py + k[1])) {
      w->rot = nr; w->px += k[0]; w->py += k[1];
      w->lockT = 0;
      return;
    }
}

static void update(float dt) {
  w->t += dt;
  switch (w->st) {
    case READY:
      if (input::pressed(B_FIRE)) { w->st = PLAY; input::clear(); }
      return;
    case OVER:
      w->overT += dt;
      if (w->overT > 0.8f && input::pressed(B_FIRE)) { reset(); w->st = PLAY; input::clear(); }
      return;
    case CLEARING:
      w->clearT -= dt;
      if (w->clearT <= 0) finishClear();
      return;
    case PLAY:
      break;
  }
  if (input::repeat(B_LEFT, 0.17f, 0.05f) && fits(w->piece, w->rot, w->px - 1, w->py)) { w->px--; w->lockT = 0; }
  if (input::repeat(B_RIGHT, 0.17f, 0.05f) && fits(w->piece, w->rot, w->px + 1, w->py)) { w->px++; w->lockT = 0; }
  if (input::pressed(B_FIRE)) rotate();
  if (input::pressed(B_RELOAD) && !w->holdUsed) {
    int cur = w->piece;
    if (w->hold < 0) { w->hold = cur; int n = w->next; w->next = fromBag(); spawn(n); }
    else { int h = w->hold; w->hold = cur; spawn(h); }
    w->holdUsed = true;
    return;
  }
  if (input::pressed(B_FWD)) {   // anında bırak
    int d = 0;
    while (fits(w->piece, w->rot, w->px, w->py + 1)) { w->py++; d++; }
    w->score += d * 2;
    lockPiece();
    return;
  }
  bool soft = input::down(B_BACK);
  float g = soft ? (gravity() < 0.04f ? gravity() : 0.04f) : gravity();
  w->fallT += dt;
  while (w->fallT >= g) {
    w->fallT -= g;
    if (fits(w->piece, w->rot, w->px, w->py + 1)) {
      w->py++;
      if (soft) w->score++;
    } else {
      break;
    }
  }
  if (!fits(w->piece, w->rot, w->px, w->py + 1)) {
    w->lockT += dt;
    if (w->lockT > 0.5f || soft) lockPiece();
  }
}

static void block(int x, int y, uint16_t c, int sz = CELL) {
  gfx::fillRect(x, y, sz - 1, sz - 1, c);
  gfx::hline(x, y, sz - 1, mix(c, C_WHITE, 120));
  gfx::vline(x, y, sz - 1, mix(c, C_WHITE, 80));
  gfx::hline(x, y + sz - 2, sz - 1, shade(c, 140));
}

static void drawPiece(int piece, int rot, int ox, int oy, int sz, uint16_t c) {
  uint16_t m = SHAPES[piece][rot];
  for (int y = 0; y < 4; y++)
    for (int x = 0; x < 4; x++)
      if (cellOf(m, x, y)) block(ox + x * sz, oy + y * sz, c, sz);
}

// Parçayı kutunun ortasına küçük çizer
static void preview(int piece, int x, int y, const char* label) {
  gfx::text(x, y, label, C_GREY);
  gfx::rect(x, y + 9, 28, 18, rgb(60, 60, 90));
  if (piece < 0) return;
  uint16_t m = SHAPES[piece][0];
  int x0 = 4, x1 = -1, y0 = 4, y1 = -1;
  for (int yy = 0; yy < 4; yy++)
    for (int xx = 0; xx < 4; xx++)
      if (cellOf(m, xx, yy)) {
        if (xx < x0) x0 = xx;
        if (xx > x1) x1 = xx;
        if (yy < y0) y0 = yy;
        if (yy > y1) y1 = yy;
      }
  const int sz = 5;
  int ox = x + 14 - (x1 - x0 + 1) * sz / 2 - x0 * sz;
  int oy = y + 18 - (y1 - y0 + 1) * sz / 2 - y0 * sz;
  drawPiece(piece, 0, ox, oy, sz, COLORS[piece + 1]);
}

static void render() {
  // arka plan
  for (int y = 0; y < SCR_H; y++) gfx::hline(0, y, SCR_W, rgb(6, 6, 18 + y / 8));
  gfx::rect(FX - 2, FY - 2, W * CELL + 3, H * CELL + 3, rgb(90, 90, 140));
  gfx::fillRect(FX - 1, FY - 1, W * CELL + 1, H * CELL + 1, rgb(4, 4, 10));
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      uint8_t c = w->field[y][x];
      if (c) block(FX + x * CELL, FY + y * CELL, COLORS[c]);
      else gfx::pixel(FX + x * CELL + 2, FY + y * CELL + 2, rgb(20, 20, 40));
    }
  if (w->st == CLEARING) {
    int a = (int)((0.3f - w->clearT) / 0.3f * 256);
    for (int i = 0; i < w->nClear; i++) {
      int ry = FY + w->clearRows[i] * CELL;
      uint16_t c = fmodf(w->clearT, 0.1f) < 0.05f ? C_WHITE : rgb(255, 230, 120);
      int half = (int)(W * CELL / 2 * (a / 256.0f));
      gfx::fillRect(FX, ry, W * CELL, CELL - 1, c);
      gfx::fillRect(FX + W * CELL / 2 - half, ry, half * 2, CELL - 1, rgb(4, 4, 10));
    }
  }
  if (w->st == PLAY) {
    int gy = w->py;
    while (fits(w->piece, w->rot, w->px, gy + 1)) gy++;
    uint16_t m = SHAPES[w->piece][w->rot];
    for (int y = 0; y < 4; y++)
      for (int x = 0; x < 4; x++)
        if (cellOf(m, x, y) && gy + y >= 0)
          gfx::rect(FX + (w->px + x) * CELL, FY + (gy + y) * CELL, CELL - 1, CELL - 1, shade(COLORS[w->piece + 1], 110));
    for (int y = 0; y < 4; y++)
      for (int x = 0; x < 4; x++)
        if (cellOf(m, x, y) && w->py + y >= 0)
          block(FX + (w->px + x) * CELL, FY + (w->py + y) * CELL, COLORS[w->piece + 1]);
  }
  // yan panel
  int sx = 70;   // yan panel solu
  gfx::textShadow(sx, 4, "BLOKLAR", rgb(120, 200, 255));
  preview(w->next, 68, 16, "SIRA");
  preview(w->hold, 98, 16, "TUT");
  gfx::text(sx, 52, "SKOR", C_GREY);
  gfx::textf(sx, 61, C_WHITE, 1, AL_LEFT, "%d", w->score);
  gfx::text(sx, 74, "SATIR", C_GREY);
  gfx::textf(sx, 83, C_WHITE, 1, AL_LEFT, "%d", w->lines);
  gfx::text(sx, 96, "SEVIYE", C_GREY);
  gfx::textf(sx, 105, rgb(255, 200, 60), 1, AL_LEFT, "%d", w->level);
  gfx::textf(sx, 118, rgb(150, 150, 190), 1, AL_LEFT, "EN %d", w->best);

  if (w->st == READY) ui::banner("BLOKLAR", "ATES: basla", rgb(120, 200, 255));
  else if (w->st == OVER && w->overT > 0.5f) {
    char b[24];
    snprintf(b, sizeof(b), w->newBest ? "REKOR! %d" : "Skor %d", w->score);
    ui::banner("BITTI", b, w->newBest ? rgb(255, 210, 40) : rgb(255, 80, 60));
    gfx::text(SCR_W / 2, 84, "ATES: tekrar", C_GREY, 1, AL_CENTER);
  }
}

static bool enter() {
  void* m = calloc(1, sizeof(World));
  if (!m) return false;
  w = new (m) World;
  w->best = save::get("tet_hi", 0);
  reset();
  w->st = READY;
  return true;
}

static void exitGame() {
  free(w);
  w = nullptr;
}

static void icon(int x, int y, float t) {
  block(x + 1, y + 18, COLORS[6], 6); block(x + 7, y + 18, COLORS[6], 6);
  block(x + 1, y + 12, COLORS[6], 6); block(x + 13, y + 18, COLORS[4], 6);
  block(x + 19, y + 18, COLORS[4], 6); block(x + 19, y + 12, COLORS[4], 6);
  int drop = t > 0 ? ((int)(t * 6) % 7) : 0;      // düşen T parçası
  block(x + 7, y + drop, COLORS[3], 6); block(x + 13, y + drop, COLORS[3], 6);
  block(x + 19, y + drop, COLORS[3], 6); block(x + 13, y + 6 + drop, COLORS[3], 6);
}

static void scoreText(char* buf, int n) {
  snprintf(buf, n, "En iyi: %d", (int)save::get("tet_hi", 0));
}

static const char* const CONTROLS[] = {
  "SOL/SAG", "Kaydir",
  "ATES", "Dondur",
  "GERI", "Hizli dusur",
  "ILERI", "Aninda birak",
  "RELOAD", "Parca beklet",
  nullptr, nullptr,
};

}  // namespace tetris

extern const Game TETRIS_GAME = {
  "BLOKLAR", tetris::icon, tetris::scoreText, tetris::CONTROLS,
  tetris::enter, tetris::update, tetris::render, tetris::exitGame,
};
