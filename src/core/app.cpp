// Uygulama döngüsü: menü -> kontroller ekranı -> oyun -> (SOLA+SAĞA 1.5 sn) onay -> menü
#include <stdio.h>
#include "display.h"
#include "game.h"
#include "gfx.h"
#include "hw.h"
#include "input.h"
#include "save.h"
#include "sd.h"
#include "ui.h"
#include "../menu/menu.h"

namespace app {

enum Mode { M_MENU, M_CONTROLS, M_PLAY, M_CONFIRM };

static const float COMBO_TIME = 1.5f;

static Mode mode = M_MENU;
static const Game* cur = nullptr;
static float modeT, clock;
static uint32_t lastUs, fpsTimer;
static int frames, fps;
static bool fpsOn, menuRequested, comboArmed = true;

float time() { return clock; }
bool showFps() { return fpsOn; }
void setShowFps(bool on) { fpsOn = on; save::set("fps", on ? 1 : 0); }
void requestMenu() { menuRequested = true; }

static void setMode(Mode m) {
  mode = m;
  modeT = 0;
  input::clear();
}

static void leaveGame() {
  if (cur) cur->exit();
  hwLog("Oyundan cikildi: %s | heap %u", cur ? cur->name : "-", (unsigned)hwFreeHeap());
  cur = nullptr;
  menuRequested = false;
  menu::enter();
  setMode(M_MENU);
}

void init() {
  hwInit();
  if (!displayInit()) {
    hwLog("Framebuffer ayrilamadi!");
    while (true) {}
  }
  hwInitButtons();   // tft.init() sonrasında
  save::init();
  fpsOn = save::get("fps", 0) != 0;
  sd::init();
  menu::init();
  hwLog("Basladi | heap %u", (unsigned)hwFreeHeap());
  lastUs = hwUs();
  fpsTimer = hwMs();
}

static void drawOverlays() {
  if (mode == M_PLAY) {
    float c = input::comboHeld();
    if (comboArmed && c > 0.3f) {
      ui::bar(24, 0, 80, 3, (c - 0.3f) / (COMBO_TIME - 0.3f), C_RED, C_BLACK);
    }
  }
  if (fpsOn) {
    char b[8];
    snprintf(b, sizeof(b), "%d", fps);
    int w = gfx::textWidth(b) + 3;
    gfx::fillRect(SCR_W - w, 0, w, 9, C_BLACK);
    gfx::text(SCR_W - 1, 1, b, C_GREEN, 1, AL_RIGHT);
  }
}

void frame() {
  uint32_t now = hwUs();
  float dt = (now - lastUs) * 1e-6f;
  lastUs = now;
  if (dt > 0.05f) dt = 0.05f;
  clock += dt;
  modeT += dt;
  input::update(hwMs());
  if (input::comboHeld() == 0) comboArmed = true;

  switch (mode) {
    case M_MENU: {
      int sel = menu::update(dt);
      menu::render();
      if (sel >= 0 && sel < GAME_COUNT) {
        cur = GAMES[sel];
        setMode(M_CONTROLS);
      }
      break;
    }
    case M_CONTROLS:
      ui::controlsScreen(cur, modeT);
      if (input::pressed(B_FIRE)) {
        ui::loadingScreen(cur->name);
        uint32_t before = hwFreeHeap();
        if (!cur->enter()) {
          cur->exit();
          ui::errorScreen("Bellek yetersiz");
          uint32_t t0 = hwMs();
          while (hwMs() - t0 < 1500) {}
          cur = nullptr;
          menu::enter();
          setMode(M_MENU);
        } else {
          hwLog("Oyun: %s | ayrilan %d B | heap %u", cur->name,
                (int)(before - hwFreeHeap()), (unsigned)hwFreeHeap());
          setMode(M_PLAY);
        }
        lastUs = hwUs();
        return;
      }
      if (input::pressed(B_RELOAD) || input::pressed(B_BACK)) {
        cur = nullptr;
        menu::enter();
        setMode(M_MENU);
      }
      break;
    case M_PLAY:
      if (comboArmed && input::comboHeld() >= COMBO_TIME) {
        comboArmed = false;
        gfx::darken(0, 0, SCR_W, SCR_H, 90);
        setMode(M_CONFIRM);
        break;
      }
      cur->update(dt);
      if (menuRequested) { leaveGame(); return; }
      cur->render();
      break;
    case M_CONFIRM:
      ui::confirmBox("Menuye don?", "ATES: Evet", "GERI: Hayir");
      if (input::pressed(B_FIRE)) { leaveGame(); return; }
      if (input::pressed(B_BACK) || input::pressed(B_RELOAD)) setMode(M_PLAY);
      break;
  }

  gfx::setClipBottom(SCR_H);   // bir oyun kırpmayı açık bıraktıysa
  drawOverlays();
  displayPush();

  frames++;
  uint32_t ms = hwMs();
  if (ms - fpsTimer >= 1000) {
    fps = frames;
    hwLog("FPS %d | heap %u (min %u, en buyuk blok %u) | %s", fps, (unsigned)hwFreeHeap(),
          (unsigned)hwMinFreeHeap(), (unsigned)hwMaxAllocHeap(), cur ? cur->name : "menu");
    frames = 0;
    fpsTimer = ms;
  }
}

}  // namespace app
