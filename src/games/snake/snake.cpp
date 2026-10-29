// YILAN — klasik yılan: giderek hızlanır, ara sıra süreli bonus yem çıkar.
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

namespace snake {

static const int CELL = 6;
static const int COLS = 21, ROWS = 19;
static const int OX = 1, OY = 13;
static const int MAXLEN = COLS * ROWS;

enum State : uint8_t { READY, PLAY, PAUSE, DEAD };

struct World {
  uint8_t body[MAXLEN][2];   // halka tampon: baş = body[head]
  uint8_t occ[ROWS][COLS];
  int head, len, grow;
  int8_t dx, dy;
  int8_t queue[2][2];
  int nq;
  int foodX, foodY, bonusX, bonusY;
  float bonusT;
  int eaten, score, best;
  float stepT, interval, t, deadT;
  State st;
  bool newBest;
};
static World* w = nullptr;

static int irand(int n) { return (int)(hwRandom() % (uint32_t)n); }

static void placeFood(int& fx, int& fy) {
  for (int tries = 0; tries < 500; tries++) {
    int x = irand(COLS), y = irand(ROWS);
    if (w->occ[y][x] || (x == w->foodX && y == w->foodY) || (x == w->bonusX && y == w->bonusY)) continue;
    fx = x; fy = y;
    return;
  }
  fx = fy = -1;
}

static void reset() {
  memset(w->occ, 0, sizeof(w->occ));
  w->len = 4; w->head = 3; w->grow = 0;
  for (int i = 0; i < 4; i++) {
    w->body[i][0] = (uint8_t)(6 + i);
    w->body[i][1] = ROWS / 2;
    w->occ[ROWS / 2][6 + i] = 1;
  }
  w->dx = 1; w->dy = 0; w->nq = 0;
  w->foodX = w->foodY = w->bonusX = w->bonusY = -1;
  w->bonusT = 0;
  placeFood(w->foodX, w->foodY);
  w->eaten = w->score = 0;
  w->interval = 0.16f;
  w->stepT = 0;
  w->newBest = false;
}

static void queueDir(int8_t dx, int8_t dy) {
  int8_t lx = w->nq ? w->queue[w->nq - 1][0] : w->dx;
  int8_t ly = w->nq ? w->queue[w->nq - 1][1] : w->dy;
  if ((dx == -lx && dy == -ly) || (dx == lx && dy == ly) || w->nq >= 2) return;
  w->queue[w->nq][0] = dx;
  w->queue[w->nq][1] = dy;
  w->nq++;
}

static void die() {
  w->st = DEAD;
  w->deadT = 0;
  if (w->score > 0 && save::setMax("snk_hi", w->score)) { w->best = w->score; w->newBest = true; }
  input::clear();
}

static void step() {
  if (w->nq) {
    w->dx = w->queue[0][0]; w->dy = w->queue[0][1];
    w->queue[0][0] = w->queue[1][0]; w->queue[0][1] = w->queue[1][1];
    w->nq--;
  }
  int hx = w->body[w->head][0] + w->dx, hy = w->body[w->head][1] + w->dy;
  if (hx < 0 || hy < 0 || hx >= COLS || hy >= ROWS) { die(); return; }
  int tail = (w->head - w->len + 1 + MAXLEN) % MAXLEN;
  bool growing = w->grow > 0;
  if (!growing) w->occ[w->body[tail][1]][w->body[tail][0]] = 0;   // kuyruk çekilir
  if (w->occ[hy][hx]) { die(); return; }
  if (growing) { w->grow--; w->len++; }
  w->head = (w->head + 1) % MAXLEN;
  w->body[w->head][0] = (uint8_t)hx;
  w->body[w->head][1] = (uint8_t)hy;
  w->occ[hy][hx] = 1;

  if (hx == w->foodX && hy == w->foodY) {
    w->eaten++;
    w->score += 10 + w->eaten / 5 * 2;
    w->grow += 2;
    w->interval = 0.16f - w->eaten * 0.0035f;
    if (w->interval < 0.055f) w->interval = 0.055f;
    w->foodX = -1;
    placeFood(w->foodX, w->foodY);
    if (w->bonusT <= 0 && w->eaten % 5 == 0) { placeFood(w->bonusX, w->bonusY); w->bonusT = 6.0f; }
  } else if (w->bonusT > 0 && hx == w->bonusX && hy == w->bonusY) {
    w->score += 50 + (int)(w->bonusT * 10);
    w->grow += 4;
    w->bonusT = 0;
    w->bonusX = w->bonusY = -1;
  }
  if (w->len >= MAXLEN - 1) die();
}

static void update(float dt) {
  w->t += dt;
  switch (w->st) {
    case READY:
      if (input::pressed(B_FIRE)) { w->st = PLAY; input::clear(); }
      return;
    case PAUSE:
      if (input::pressed(B_FIRE) || input::pressed(B_RELOAD)) w->st = PLAY;
      return;
    case DEAD:
      w->deadT += dt;
      if (w->deadT > 0.8f && input::pressed(B_FIRE)) { reset(); w->st = PLAY; input::clear(); }
      return;
    case PLAY:
      break;
  }
  if (input::pressed(B_FWD))   queueDir(0, -1);
  if (input::pressed(B_BACK))  queueDir(0, 1);
  if (input::pressed(B_LEFT))  queueDir(-1, 0);
  if (input::pressed(B_RIGHT)) queueDir(1, 0);
  if (input::pressed(B_FIRE) || input::pressed(B_RELOAD)) { w->st = PAUSE; return; }
  if (w->bonusT > 0) { w->bonusT -= dt; if (w->bonusT <= 0) w->bonusX = w->bonusY = -1; }
  w->stepT += dt;
  while (w->stepT >= w->interval && w->st == PLAY) {
    w->stepT -= w->interval;
    step();
  }
}

static void drawApple(int x, int y, bool gold, float t) {
  int px = OX + x * CELL, py = OY + y * CELL;
  uint16_t c = gold ? rgb(255, 210, 40) : rgb(220, 30, 30);
  if (gold && fmodf(t, 0.3f) < 0.1f) c = C_WHITE;
  gfx::fillCircle(px + 3, py + 3, 2, c);
  gfx::pixel(px + 2, py + 2, gold ? C_WHITE : rgb(255, 150, 150));
  gfx::pixel(px + 4, py, rgb(60, 200, 60));
}

static void render() {
  // arka plan: koyu dama
  gfx::fillRect(0, 0, SCR_W, OY - 1, C_BLACK);
  for (int y = 0; y < ROWS; y++)
    for (int x = 0; x < COLS; x++)
      gfx::fillRect(OX + x * CELL, OY + y * CELL, CELL, CELL, ((x + y) & 1) ? rgb(8, 22, 12) : rgb(12, 28, 16));
  gfx::rect(OX - 1, OY - 1, COLS * CELL + 2, ROWS * CELL + 2, rgb(40, 120, 60));

  if (w->foodX >= 0) drawApple(w->foodX, w->foodY, false, w->t);
  if (w->bonusT > 0 && (w->bonusT > 1.5f || fmodf(w->t, 0.2f) < 0.12f)) drawApple(w->bonusX, w->bonusY, true, w->t);

  bool flash = w->st == DEAD && w->deadT < 0.8f && fmodf(w->deadT, 0.16f) < 0.08f;
  for (int i = 0; i < w->len; i++) {
    int idx = (w->head - i + MAXLEN) % MAXLEN;
    int px = OX + w->body[idx][0] * CELL, py = OY + w->body[idx][1] * CELL;
    int g = 230 - i * 140 / (w->len + 1);
    uint16_t c = flash ? C_WHITE : rgb(40, g, 60);
    gfx::fillRect(px, py, CELL - 1, CELL - 1, c);
    if (i == 0) {
      gfx::fillRect(px, py, CELL - 1, CELL - 1, flash ? C_WHITE : rgb(120, 255, 120));
      int ex = w->dx, ey = w->dy;
      gfx::pixel(px + 2 + ex - ey, py + 2 + ey + ex, C_BLACK);
      gfx::pixel(px + 2 + ex + ey, py + 2 + ey - ex, C_BLACK);
    }
  }

  gfx::textf(2, 2, C_WHITE, 1, AL_LEFT, "SKOR %d", w->score);
  gfx::textf(SCR_W - 2, 2, rgb(255, 200, 60), 1, AL_RIGHT, "EN %d", w->best);
  if (w->bonusT > 0) ui::bar(52, 4, 24, 3, w->bonusT / 6.0f, rgb(255, 210, 40), rgb(40, 30, 0));

  if (w->st == READY) {
    ui::banner("YILAN", "ATES: basla", rgb(120, 255, 120));
  } else if (w->st == PAUSE) {
    ui::banner("DURDU", "ATES: devam", C_WHITE);
  } else if (w->st == DEAD && w->deadT > 0.8f) {
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
  w->best = save::get("snk_hi", 0);
  reset();
  w->st = READY;
  return true;
}

static void exitGame() {
  free(w);
  w = nullptr;
}

static void icon(int x, int y, float t) {
  static const uint8_t PATH[][2] = { {2, 18}, {2, 14}, {2, 10}, {6, 10}, {10, 10}, {14, 10}, {14, 6}, {18, 6} };
  int n = sizeof(PATH) / sizeof(PATH[0]);
  int shift = t > 0 ? ((int)(t * 6) % 3) : 0;
  for (int i = 0; i < n; i++)
    gfx::fillRect(x + PATH[i][0], y + PATH[i][1] - (i == n - 1 ? shift / 2 : 0), 4, 4,
                  i == n - 1 ? rgb(120, 255, 120) : rgb(40, 200 - i * 10, 60));
  gfx::fillCircle(x + 19, y + 18, 3, rgb(220, 30, 30));
  gfx::pixel(x + 20, y + 14, rgb(60, 200, 60));
}

static void scoreText(char* buf, int n) {
  snprintf(buf, n, "En iyi: %d", (int)save::get("snk_hi", 0));
}

static const char* const CONTROLS[] = {
  "ILERI", "Yukari",
  "GERI", "Asagi",
  "SOLA", "Sola",
  "SAGA", "Saga",
  "ATES", "Duraklat",
  nullptr, nullptr,
};

}  // namespace snake

extern const Game SNAKE_GAME = {
  "YILAN", snake::icon, snake::scoreText, snake::CONTROLS,
  snake::enter, snake::update, snake::render, snake::exitGame,
};
