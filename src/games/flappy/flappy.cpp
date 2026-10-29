// FLAPPY — gece mağarasında uçan yarasa: parallax arka plan, kayan kemik sütunlar,
// zorluk (hız, aralık, hareketli sütunlar) skorla artar.
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

namespace flappy {

static const int GROUND = 116;
static const int BAT_X = 34;
static const int PIPE_W = 14;
static const int NPIPES = 4;
static const float GRAV = 390.0f, FLAP = -128.0f;

enum State : uint8_t { READY, PLAY, PAUSE, DEAD };

struct Pipe { float x; float gapY; float phase; bool scored; };

struct World {
  Pipe pipes[NPIPES];
  float y, vy, t, scroll, deadT, flapT;
  int score, best;
  State st;
  bool newBest;
  uint8_t hills[SCR_W];   // uzak tepe silueti (periyodik)
};
static World* w = nullptr;

static float frand() { return (hwRandom() & 0xFFFF) / 65536.0f; }

static float speed() { float s = 42 + w->score * 1.3f; return s > 78 ? 78 : s; }
static int gapSize() { int g = 54 - w->score; return g < 36 ? 36 : g; }

static void placePipe(Pipe& p, float x) {
  p.x = x;
  int g = gapSize();
  p.gapY = 14 + g / 2 + frand() * (GROUND - 28 - g);
  p.phase = frand() * 6.28f;
  p.scored = false;
}

static void reset() {
  w->y = 56; w->vy = 0; w->score = 0; w->newBest = false;
  for (int i = 0; i < NPIPES; i++) placePipe(w->pipes[i], SCR_W + 30 + i * 44.0f);
}

static float pipeGap(const Pipe& p) {
  if (w->score < 10) return p.gapY;
  float amp = w->score < 20 ? 8.0f : 14.0f;   // 10 puandan sonra sütunlar oynar
  float g = p.gapY + sinf(w->t * 1.6f + p.phase) * amp;
  float half = gapSize() / 2.0f;
  if (g < 10 + half) g = 10 + half;
  if (g > GROUND - 4 - half) g = GROUND - 4 - half;
  return g;
}

static void die() {
  w->st = DEAD;
  w->deadT = 0;
  if (w->score > 0 && save::setMax("flp_hi", w->score)) { w->best = w->score; w->newBest = true; }
  input::clear();
}

static void update(float dt) {
  w->t += dt;
  if (w->flapT > 0) w->flapT -= dt;
  bool flap = input::pressed(B_FIRE) || input::pressed(B_FWD);
  switch (w->st) {
    case READY:
      w->scroll += 30 * dt;
      w->y = 56 + sinf(w->t * 4) * 4;
      if (flap) { w->st = PLAY; w->vy = FLAP; w->flapT = 0.15f; }
      return;
    case PAUSE:
      if (input::pressed(B_BACK) || input::pressed(B_RELOAD) || flap) { w->st = PLAY; input::clear(); }
      return;
    case DEAD:
      w->deadT += dt;
      if (w->y < GROUND - 4) { w->vy += GRAV * dt; w->y += w->vy * dt; }
      if (w->deadT > 0.9f && flap) { reset(); w->st = READY; input::clear(); }
      return;
    case PLAY:
      break;
  }
  if (input::pressed(B_BACK) || input::pressed(B_RELOAD)) { w->st = PAUSE; return; }
  if (flap) { w->vy = FLAP; w->flapT = 0.15f; }
  w->vy += GRAV * dt;
  if (w->vy > 220) w->vy = 220;
  w->y += w->vy * dt;
  if (w->y < 2) { w->y = 2; w->vy = 0; }
  float sp = speed();
  w->scroll += sp * dt;
  float maxX = 0;
  for (Pipe& p : w->pipes) if (p.x > maxX) maxX = p.x;
  for (Pipe& p : w->pipes) {
    p.x -= sp * dt;
    if (p.x < -PIPE_W) placePipe(p, maxX + 44 + frand() * 10);
    if (!p.scored && p.x + PIPE_W < BAT_X) { p.scored = true; w->score++; }
    // çarpışma (yarasa ~8x6)
    if (BAT_X + 4 > p.x && BAT_X - 4 < p.x + PIPE_W) {
      float g = pipeGap(p), half = gapSize() / 2.0f;
      if (w->y - 3 < g - half || w->y + 3 > g + half) die();
    }
  }
  if (w->y > GROUND - 4) { w->y = GROUND - 4; die(); }
}

static void drawBat(int x, int y, bool up, bool dead) {
  uint16_t body = dead ? rgb(150, 80, 80) : rgb(170, 130, 190);
  uint16_t wing = dead ? rgb(110, 50, 50) : rgb(120, 85, 150);
  if (up) {
    gfx::fillTriangle(x - 2, y, x - 9, y - 6, x - 6, y + 1, wing);
    gfx::fillTriangle(x + 2, y, x + 9, y - 6, x + 6, y + 1, wing);
  } else {
    gfx::fillTriangle(x - 2, y, x - 9, y + 5, x - 5, y + 1, wing);
    gfx::fillTriangle(x + 2, y, x + 9, y + 5, x + 5, y + 1, wing);
  }
  gfx::fillEllipse(x, y, 3, 3, body);
  gfx::pixel(x - 2, y - 4, body);
  gfx::pixel(x + 2, y - 4, body);
  gfx::pixel(x - 1, y - 1, dead ? C_WHITE : rgb(255, 40, 30));
  gfx::pixel(x + 1, y - 1, dead ? C_WHITE : rgb(255, 40, 30));
}

static void drawPipe(int x, float gap, int half) {
  int top = (int)(gap - half), bot = (int)(gap + half);
  uint16_t bone = rgb(200, 190, 165), dark = rgb(120, 110, 95);
  gfx::fillRect(x, 0, PIPE_W, top, bone);
  gfx::fillRect(x, bot, PIPE_W, GROUND - bot, bone);
  gfx::vline(x + 2, 0, top, C_WHITE);
  gfx::vline(x + 2, bot, GROUND - bot, C_WHITE);
  gfx::vline(x + PIPE_W - 3, 0, top, dark);
  gfx::vline(x + PIPE_W - 3, bot, GROUND - bot, dark);
  // boğum uçları ve kafatası
  gfx::fillRect(x - 2, top - 5, PIPE_W + 4, 5, dark);
  gfx::fillRect(x - 2, bot, PIPE_W + 4, 5, dark);
  for (int s = 0; s < 2; s++) {
    int sy = s == 0 ? top - 13 : bot + 7;
    if (sy < 0 || sy > GROUND - 8) continue;
    gfx::fillEllipse(x + PIPE_W / 2, sy + 3, 4, 3, rgb(225, 220, 200));
    gfx::pixel(x + PIPE_W / 2 - 2, sy + 3, C_BLACK);
    gfx::pixel(x + PIPE_W / 2 + 2, sy + 3, C_BLACK);
  }
}

static void render() {
  // gökyüzü
  gfx::vgradient(0, 0, SCR_W, GROUND, rgb(20, 10, 45), rgb(70, 25, 50));
  // yıldızlar (yavaş kayar)
  for (int i = 0; i < 24; i++) {
    int sx = (int)(hash32(i * 7 + 1) % 256 - w->scroll * 0.05f);
    sx = ((sx % SCR_W) + SCR_W) % SCR_W;
    int sy = (int)(hash32(i * 13 + 5) % 60);
    gfx::pixel(sx, sy, (hash32(i + (int)(w->t * 3)) & 7) ? rgb(150, 140, 180) : C_WHITE);
  }
  // ay
  gfx::fillCircle(96, 22, 10, rgb(230, 220, 190));
  gfx::fillCircle(92, 19, 9, rgb(30, 14, 50));
  // uzak tepeler (parallax 0.2), yakın ağaçlar (0.5)
  for (int x = 0; x < SCR_W; x++) {
    int hx = (x + (int)(w->scroll * 0.2f)) & (SCR_W - 1);
    int h = w->hills[hx];
    gfx::vline(x, GROUND - h, h, rgb(35, 15, 45));
  }
  for (int i = 0; i < 6; i++) {
    int tx = (int)(i * 26 - fmodf(w->scroll * 0.5f, 26.0f * 6)) ;
    if (tx < -20) tx += 26 * 6;
    int th = 18 + (int)(hash32(i + 99) % 14);
    gfx::fillTriangle(tx, GROUND, tx + 7, GROUND - th, tx + 14, GROUND, rgb(18, 8, 24));
    gfx::vline(tx + 7, GROUND - th - 4, 6, rgb(18, 8, 24));
  }
  int half = gapSize() / 2;
  for (const Pipe& p : w->pipes) if (p.x < SCR_W && p.x > -PIPE_W - 4) drawPipe((int)p.x, pipeGap(p), half);
  // zemin (hızlı kayar)
  gfx::fillRect(0, GROUND, SCR_W, SCR_H - GROUND, rgb(40, 25, 20));
  gfx::hline(0, GROUND, SCR_W, rgb(90, 60, 40));
  for (int x = -(int)fmodf(w->scroll, 8.0f); x < SCR_W; x += 8) gfx::line(x, GROUND + 2, x + 4, SCR_H - 1, rgb(60, 40, 30));

  bool up = w->flapT > 0 || (w->st == READY && fmodf(w->t, 0.3f) < 0.15f);
  drawBat(BAT_X, (int)w->y, up, w->st == DEAD);

  char b[12];
  snprintf(b, sizeof(b), "%d", w->score);
  gfx::textShadow(SCR_W / 2, 6, b, C_WHITE, 2, AL_CENTER);
  if (w->st == READY) {
    gfx::textShadow(SCR_W / 2, 74, "ATES: kanat cirp", rgb(255, 200, 120), 1, AL_CENTER);
    gfx::textf(SCR_W / 2, 86, rgb(170, 150, 190), 1, AL_CENTER, "En iyi %d", w->best);
  } else if (w->st == PAUSE) {
    ui::banner("DURDU", "ATES: devam", C_WHITE);
  } else if (w->st == DEAD && w->deadT > 0.6f) {
    char s[24];
    snprintf(s, sizeof(s), w->newBest ? "REKOR! %d" : "Skor %d  En iyi %d", w->score, w->best);
    ui::banner("DUSTUN", s, w->newBest ? rgb(255, 210, 40) : rgb(255, 80, 60));
    if (w->deadT > 0.9f) gfx::text(SCR_W / 2, 84, "ATES: tekrar", C_GREY, 1, AL_CENTER);
  }
}

static bool enter() {
  void* m = calloc(1, sizeof(World));
  if (!m) return false;
  w = new (m) World;
  for (int x = 0; x < SCR_W; x++) {
    float a = x * 6.2832f / SCR_W;
    w->hills[x] = (uint8_t)(22 + sinf(a * 2) * 8 + sinf(a * 5 + 1) * 4 + sinf(a * 11) * 2);
  }
  w->best = save::get("flp_hi", 0);
  reset();
  w->st = READY;
  return true;
}

static void exitGame() {
  free(w);
  w = nullptr;
}

static void icon(int x, int y, float t) {
  gfx::fillRect(x, y, 24, 24, rgb(25, 12, 45));
  gfx::fillCircle(x + 18, y + 5, 3, rgb(230, 220, 190));
  gfx::fillRect(x + 1, y, 5, 8, rgb(200, 190, 165));
  gfx::fillRect(x + 1, y + 16, 5, 8, rgb(200, 190, 165));
  int by = y + 12 + (t > 0 ? (int)(sinf(t * 8) * 3) : 0);
  bool up = t > 0 && fmodf(t, 0.3f) < 0.15f;
  drawBat(x + 14, by, up, false);
}

static void scoreText(char* buf, int n) {
  snprintf(buf, n, "En iyi: %d", (int)save::get("flp_hi", 0));
}

static const char* const CONTROLS[] = {
  "ATES", "Kanat cirp",
  "ILERI", "Kanat cirp",
  "GERI", "Duraklat",
  nullptr, nullptr,
};

}  // namespace flappy

extern const Game FLAPPY_GAME = {
  "FLAPPY", flappy::icon, flappy::scoreText, flappy::CONTROLS,
  flappy::enter, flappy::update, flappy::render, flappy::exitGame,
};
