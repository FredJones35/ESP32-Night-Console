#include "menu.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../core/game.h"
#include "../core/gfx.h"
#include "../core/input.h"
#include "../core/save.h"
#include "../core/sd.h"
#include "../core/ui.h"

namespace menu {

enum Screen { SPLASH, LIST, SETTINGS, RESET_CONFIRM };

static const int ROW_H = 30;
static const int VISIBLE = 3;
static const int LIST_Y = 15;

static Screen screen;
static float t, flash;
static ui::List list, setList;
static char scores[8][24];
static int sdCounts[3];

struct Ember { float x, y, vy, life; };
static Ember embers[20];

static void countLevels();

static void rescanSd() {
  sd::init();
  countLevels();
}

static void refreshScores() {
  for (int i = 0; i < GAME_COUNT && i < 8; i++) {
    scores[i][0] = 0;
    if (GAMES[i]->scoreText) GAMES[i]->scoreText(scores[i], sizeof(scores[i]));
  }
}

static void resetEmber(Ember& e, bool anywhere) {
  e.x = (float)(hwRandom() % SCR_W);
  e.y = anywhere ? (float)(hwRandom() % SCR_H) : SCR_H + 2.0f;
  e.vy = 6.0f + (hwRandom() % 100) * 0.15f;
  e.life = 1.0f;
}

static void countLevels() {
  sdCounts[0] = sd::count("/doom", ".lvl");
  sdCounts[1] = sd::count("/race", ".trk");
  sdCounts[2] = sd::count("/moto", ".mtb");
}

void init() {
  screen = SPLASH;
  for (Ember& e : embers) resetEmber(e, true);
  countLevels();
  refreshScores();
}

void enter() {
  screen = LIST;
  refreshScores();
}

// ---------- arka plan: sis + kor parçacıkları ----------
static void drawBackdrop(int fogBright) {
  for (int y = 0; y < SCR_H; y++) {
    // yavaşça akan sis bantları
    float f = sinf(y * 0.09f + t * 0.7f) * 0.5f + sinf(y * 0.031f - t * 0.4f) * 0.5f;
    int v = (int)((f * 0.5f + 0.5f) * fogBright) + (SCR_H - y) / 12;
    gfx::hline(0, y, SCR_W, rgb(v + 4, v / 5, v / 6));
  }
  for (Ember& e : embers) {
    int b = (int)(e.life * 255);
    gfx::pixel((int)e.x, (int)e.y, rgb(b, b / 3, 0));
  }
}

static void updateEmbers(float dt) {
  for (Ember& e : embers) {
    e.y -= e.vy * dt;
    e.x += sinf(t * 2 + e.vy) * 4 * dt;
    e.life -= dt * 0.25f;
    if (e.y < -2 || e.life <= 0) resetEmber(e, false);
  }
}

static void drawGear(int x, int y, float a) {
  int cx = x + 12, cy = y + 12;
  for (int i = 0; i < 8; i++) {
    float ang = a + i * 0.785398f;
    int tx = cx + (int)lroundf(cosf(ang) * 9), ty = cy + (int)lroundf(sinf(ang) * 9);
    gfx::fillRect(tx - 2, ty - 2, 4, 4, rgb(150, 150, 160));
  }
  gfx::fillCircle(cx, cy, 7, rgb(120, 120, 130));
  gfx::fillCircle(cx, cy, 3, rgb(20, 20, 24));
}

static void drawRow(int i, int x, int y, int w, int h, bool sel, float tt) {
  bool settings = i >= GAME_COUNT;
  uint16_t border = sel ? rgb(200 + (int)(sinf(tt * 6) * 55), 20, 10) : rgb(60, 10, 10);
  ui::panel(x, y + 1, w - 4, h - 2, border, sel ? rgb(28, 4, 4) : rgb(10, 2, 2));
  gfx::fillRect(x + 3, y + 3, 26, 26, C_BLACK);
  if (settings) drawGear(x + 4, y + 4, sel ? tt : 0);
  else GAMES[i]->icon(x + 4, y + 4, sel ? tt : 0);
  const char* name = settings ? "AYARLAR" : GAMES[i]->name;
  uint16_t nc = sel ? C_WHITE : rgb(170, 160, 160);
  gfx::text(x + 31, y + 6, name, nc);
  if (sel) gfx::text(x + 32, y + 6, name, nc);   // kalın görünüm
  const char* sub = settings ? (sd::ok() ? "SD takili" : "SD yok") : scores[i];
  gfx::text(x + 31, y + 17, sub, sel ? rgb(255, 170, 60) : rgb(110, 90, 80));
}

// ---------- Ayarlar ----------
static const int SET_COUNT = 7;
static const int SET_ROW_H = 13;

static void drawSetRow(int i, int x, int y, int w, int h, bool sel, float tt) {
  (void)tt;
  char b[28];
  switch (i) {
    case 0: snprintf(b, sizeof(b), "SD kart: %s", sd::ok() ? "TAKILI" : "YOK"); break;
    case 1: snprintf(b, sizeof(b), "Doom SD bolum: %d", sdCounts[0]); break;
    case 2: snprintf(b, sizeof(b), "Yaris SD pist: %d", sdCounts[1]); break;
    case 3: snprintf(b, sizeof(b), "Moto SD parkur: %d", sdCounts[2]); break;
    case 4: snprintf(b, sizeof(b), "FPS goster: %s", app::showFps() ? "ACIK" : "KAPALI"); break;
    case 5: snprintf(b, sizeof(b), "Kayitlari sifirla"); break;
    default: snprintf(b, sizeof(b), "Geri"); break;
  }
  if (sel) gfx::fillRect(x, y, w - 4, h - 1, rgb(60, 6, 6));
  gfx::text(x + 4, y + (h - 8) / 2, b, sel ? C_WHITE : rgb(170, 150, 150));
}

static int updateSettings(float dt) {
  int r = setList.update(SET_COUNT, dt, SET_COUNT);
  if (r == -2 || r == 6) { screen = LIST; input::clear(); }
  else if (r >= 0 && r <= 3) rescanSd();   // SD satırları: kartı yeniden tara
  else if (r == 4) app::setShowFps(!app::showFps());
  else if (r == 5) { screen = RESET_CONFIRM; input::clear(); }
  return -1;
}

static void renderSettings() {
  drawBackdrop(14);
  gfx::fillRect(0, 0, SCR_W, 14, C_BLACK);
  gfx::textShadow(SCR_W / 2, 3, "AYARLAR", rgb(255, 60, 40), 1, AL_CENTER);
  gfx::hline(0, 13, SCR_W, rgb(120, 0, 0));
  ui::listDraw(setList, SET_COUNT, 2, 17, SCR_W - 2, SET_ROW_H, SET_COUNT, drawSetRow, t);
  gfx::fillRect(0, 110, SCR_W, SCR_H - 110, C_BLACK);
  gfx::hline(0, 110, SCR_W, rgb(120, 0, 0));
  gfx::text(SCR_W / 2, 116, "ATES:sec RELOAD:geri", rgb(110, 80, 80), 1, AL_CENTER);
}

int update(float dt) {
  t += dt;
  if (flash > 0) flash -= dt;
  else if ((hwRandom() % 1000) < 3) flash = 0.12f;   // uzak şimşek
  updateEmbers(dt);

  switch (screen) {
    case SPLASH:
      if (t > 0.6f && input::anyPressed()) { screen = LIST; input::clear(); }
      return -1;
    case LIST: {
      int r = list.update(GAME_COUNT + 1, dt, VISIBLE);
      if (r == GAME_COUNT) { screen = SETTINGS; setList.sel = 0; input::clear(); return -1; }
      return r >= 0 ? r : -1;
    }
    case SETTINGS:
      return updateSettings(dt);
    case RESET_CONFIRM:
      if (input::pressed(B_FIRE)) {
        save::reset();
        app::setShowFps(false);
        refreshScores();
        screen = SETTINGS;
        input::clear();
      } else if (input::pressed(B_BACK) || input::pressed(B_RELOAD)) {
        screen = SETTINGS;
        input::clear();
      }
      return -1;
  }
  return -1;
}

static void renderSplash() {
  drawBackdrop(26);
  if (flash > 0) gfx::blend(0, 0, SCR_W, SCR_H, rgb(120, 110, 140), 60);
  // titreyen başlık
  bool flick = (hash32((uint32_t)(t * 20)) & 15) == 0;
  uint16_t title = flick ? rgb(90, 0, 0) : rgb(230, 20, 10);
  for (int g = 3; g >= 1; g--) {
    uint16_t gc = rgb(40 * (4 - g) / 2, 0, 0);
    gfx::text(SCR_W / 2 - g, 30, "GECE", gc, 3, AL_CENTER);
    gfx::text(SCR_W / 2 + g, 30, "GECE", gc, 3, AL_CENTER);
  }
  gfx::text(SCR_W / 2, 30, "GECE", title, 3, AL_CENTER);
  gfx::textShadow(SCR_W / 2, 58, "KONSOLU", flick ? rgb(80, 60, 60) : rgb(200, 190, 180), 2, AL_CENTER);
  // başlıktan damlayan kan
  for (int i = 0; i < 5; i++) {
    int x = 36 + i * 13 + (i & 1) * 3;
    float len = fmodf(t * (2 + i) + i * 3, 7.0f);
    gfx::vline(x, 52, (int)len, rgb(140, 0, 0));
    gfx::pixel(x, 52 + (int)len, rgb(220, 0, 0));
  }
  gfx::text(SCR_W / 2, 82, "ESP32 el konsolu", rgb(120, 100, 100), 1, AL_CENTER);
  gfx::text(SCR_W / 2, 92, "6 oyun", rgb(120, 100, 100), 1, AL_CENTER);
  if (fmodf(t, 1.2f) < 0.8f) gfx::text(SCR_W / 2, 112, "Bir tusa bas", rgb(255, 90, 60), 1, AL_CENTER);
}

static void renderList() {
  drawBackdrop(12);
  ui::listDraw(list, GAME_COUNT + 1, 2, LIST_Y, SCR_W - 2, ROW_H, VISIBLE, drawRow, t);
  gfx::fillRect(0, 0, SCR_W, LIST_Y - 1, C_BLACK);
  gfx::textShadow(3, 3, "GECE KONSOLU", rgb(220, 30, 20));
  uint16_t sdc = sd::ok() ? rgb(40, 160, 60) : rgb(70, 50, 50);   // küçük SD kart simgesi
  gfx::fillRect(SCR_W - 12, 3, 8, 9, sdc);
  gfx::fillRect(SCR_W - 6, 3, 2, 2, C_BLACK);
  for (int i = 0; i < 3; i++) gfx::vline(SCR_W - 11 + i * 2, 4, 2, C_BLACK);
  gfx::hline(0, LIST_Y - 1, SCR_W, rgb(120, 0, 0));
  int fy = LIST_Y + VISIBLE * ROW_H + 1;
  gfx::fillRect(0, fy, SCR_W, SCR_H - fy, C_BLACK);
  gfx::hline(0, fy, SCR_W, rgb(120, 0, 0));
  gfx::text(SCR_W / 2, fy + 6, "ILERI/GERI sec  ATES", rgb(120, 90, 90), 1, AL_CENTER);
}

void render() {
  switch (screen) {
    case SPLASH: renderSplash(); break;
    case LIST: renderList(); break;
    case SETTINGS: renderSettings(); break;
    case RESET_CONFIRM:
      renderSettings();
      gfx::darken(0, 0, SCR_W, SCR_H, 100);
      ui::confirmBox("Kayitlar silinsin?", "ATES: Evet", "GERI: Hayir");
      break;
  }
}

}  // namespace menu
