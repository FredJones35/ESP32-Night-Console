// YARIS — segment tabanlı pseudo-3D yarış: virajlar, yokuşlar, çevre objeleri,
// 5 rakip, tur süresi, sıralama, nitro, kariyer (pist açma) ve SD'den ek pistler.
#include <math.h>
#include <new>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../core/game.h"
#include "../../core/gfx.h"
#include "../../core/hw.h"
#include "../../core/input.h"
#include "../../core/levels.h"
#include "../../core/parse.h"
#include "../../core/save.h"
#include "../../core/ui.h"
#include "tracks.h"

namespace race {

static const int   MAX_SEG = 1500;
static const int   DRAW = 64;          // görüş mesafesi (segment)
static const int   NRIV = 5;
static const float SEG_L = 200.0f;
static const float ROAD_W = 1000.0f;   // yol yarı genişliği
static const float CAM_H = 1000.0f;
static const float CAM_D = 0.84f;      // ~100 derece görüş açısı
static const float PLAYER_Z = CAM_H * CAM_D;
static const float MAXSPD = SEG_L * 50;
static const float ACCEL = MAXSPD / 4.5f, BRAKE = -MAXSPD * 0.9f, DECEL = -MAXSPD / 6;
static const float OFF_DECEL = -MAXSPD / 1.8f, OFF_LIMIT = MAXSPD / 4;
static const float CENTRIFUGAL = 0.32f;
static const int   HALF = SCR_W / 2;

enum Deco : uint8_t {
  D_NONE, D_TREE, D_PINE, D_PALM, D_CACTUS, D_ROCK, D_BUSH,
  D_SIGN_L, D_SIGN_R, D_POST, D_BILLBOARD, D_LAMP, D_HOUSE, D_COUNT
};
struct DecoInfo { const char* name; float w, h; bool solid; };
static const DecoInfo DECOS[D_COUNT] = {
  { "", 0, 0, false },
  { "tree", 900, 1500, true },   { "pine", 700, 1700, true },  { "palm", 900, 1800, true },
  { "cactus", 400, 900, true },  { "rock", 700, 450, true },   { "bush", 800, 380, false },
  { "signl", 600, 650, true },   { "signr", 600, 650, true },  { "post", 120, 450, false },
  { "billboard", 1700, 1300, true }, { "lamp", 220, 1600, true }, { "house", 1800, 1300, true },
};

struct Seg { float y; int8_t curve10; uint8_t deco; int8_t decoX10; uint8_t pad; };
struct Car { float dist, x, targetX, speed, maxSpeed, laneT; uint16_t color; bool finished; };
struct Theme { uint16_t grass1, grass2, rumble1, rumble2, road1, road2, lane, skyTop, skyBot, hill, fog; };

enum Mode : uint8_t { M_SELECT, M_COUNT, M_RACE, M_PAUSE, M_RESULT };
enum ThemeId : uint8_t { T_GRASS, T_DESERT, T_SNOW, T_CITY };
enum SkyId : uint8_t { S_DAY, S_DUSK, S_NIGHT };

struct World {
  LevelSet tracks;
  ui::List list;
  int unlocked, trackIdx;
  Mode mode;
  float modeT;

  Seg seg[MAX_SEG];
  int n;
  float trackLen, lastY;
  char name[20];
  int laps;
  float skill, density;
  uint8_t theme, sky;
  uint8_t decoTypes[4];
  int nDecoTypes;
  Theme th;

  Car cars[NRIV];
  float dist, speed, px, steer, lapTime, raceTime, bestLap, lastLap, nitroT, skyOff, crashT, msgT;
  int lap, nitros, place;
  bool finished, newBestLap, unlockedNew;
  char msg[24];

  // projeksiyon önbelleği (kenar başına)
  float ex[DRAW + 1], ey[DRAW + 1], ew[DRAW + 1], es[DRAW + 1];
  int eclip[DRAW + 1];
  bool evis[DRAW + 1];
};
static World* w = nullptr;

static inline float frand() { return (hwRandom() & 0xFFFFFF) / 16777216.0f; }
static inline int idx(int i) { return ((i % w->n) + w->n) % w->n; }
static inline float curveOf(int i) { return w->seg[idx(i)].curve10 * 0.1f; }
static inline float yOf(int i) { return w->seg[idx(i)].y; }

// ---------- pist kurma ----------
static float easeIn(float a, float b, float p) { return a + (b - a) * p * p; }
static float easeOut(float a, float b, float p) { return a + (b - a) * (1 - (1 - p) * (1 - p)); }
static float easeInOut(float a, float b, float p) { return a + (b - a) * (-cosf(p * 3.14159f) / 2 + 0.5f); }

static void addSegment(float curve, float y) {
  if (w->n >= MAX_SEG) return;
  Seg& s = w->seg[w->n++];
  s.y = y;
  int c = (int)lroundf(curve * 10);
  s.curve10 = (int8_t)(c > 100 ? 100 : c < -100 ? -100 : c);
  s.deco = D_NONE;
  s.decoX10 = 0;
}

static void addSection(int len, float curve, float hill) {
  if (len < 3) len = 3;
  int enter = len / 4, hold = len / 2, leave = len - enter - hold;
  float y0 = w->lastY, y1 = y0 + hill;
  for (int i = 0; i < len; i++) {
    float c;
    if (i < enter) c = easeIn(0, curve, (float)i / enter);
    else if (i < enter + hold) c = curve;
    else c = easeOut(curve, 0, (float)(i - enter - hold) / leave);
    addSegment(c, easeInOut(y0, y1, (float)i / len));
  }
  w->lastY = y1;
}

static Deco decoByName(const char* s) {
  for (int i = 1; i < D_COUNT; i++) if (streq(s, DECOS[i].name)) return (Deco)i;
  return D_NONE;
}

static void placeDeco(int i, Deco d, float x) {
  Seg& s = w->seg[idx(i)];
  s.deco = d;
  int v = (int)lroundf(x * 10);
  s.decoX10 = (int8_t)(v > 60 ? 60 : v < -60 ? -60 : v);
}

static void setTheme() {
  Theme& t = w->th;
  switch (w->theme) {
    case T_DESERT:
      t = { rgb(220, 180, 110), rgb(205, 165, 95), rgb(200, 70, 40), rgb(240, 220, 180),
            rgb(118, 104, 94), rgb(110, 97, 88), rgb(240, 240, 220), 0, 0, rgb(150, 80, 60), 0 };
      break;
    case T_SNOW:
      t = { rgb(235, 240, 250), rgb(212, 222, 238), rgb(40, 90, 200), rgb(240, 240, 255),
            rgb(118, 123, 134), rgb(110, 115, 126), rgb(255, 255, 255), 0, 0, rgb(170, 185, 210), 0 };
      break;
    case T_CITY:
      t = { rgb(40, 52, 40), rgb(34, 45, 34), rgb(210, 200, 40), rgb(45, 45, 45),
            rgb(62, 62, 70), rgb(57, 57, 65), rgb(240, 210, 40), 0, 0, rgb(30, 30, 48), 0 };
      break;
    default:
      t = { rgb(72, 165, 45), rgb(60, 145, 38), rgb(200, 30, 30), rgb(235, 235, 235),
            rgb(108, 108, 112), rgb(100, 100, 104), rgb(245, 245, 245), 0, 0, rgb(55, 120, 70), 0 };
      break;
  }
  switch (w->sky) {
    case S_DUSK:  t.skyTop = rgb(70, 30, 100); t.skyBot = rgb(250, 140, 70); t.fog = rgb(200, 120, 90); break;
    case S_NIGHT: t.skyTop = rgb(4, 4, 18); t.skyBot = rgb(40, 20, 60); t.fog = rgb(15, 10, 30); break;
    default:      t.skyTop = rgb(50, 120, 220); t.skyBot = rgb(175, 215, 250); t.fog = rgb(170, 200, 230); break;
  }
  if (w->sky == S_NIGHT) {   // gece zemin karanlık
    t.grass1 = shade(t.grass1, 110); t.grass2 = shade(t.grass2, 110);
    t.road1 = shade(t.road1, 150); t.road2 = shade(t.road2, 150);
    t.rumble1 = shade(t.rumble1, 170); t.rumble2 = shade(t.rumble2, 170);
    t.hill = shade(t.hill, 120);
  }
}

static bool buildTrack(int ti) {
  bool owned;
  const char* txt = w->tracks.load(ti, &owned);
  if (!txt) return false;
  w->n = 0;
  w->lastY = 0;
  w->laps = 3; w->skill = 0.9f; w->density = 0.35f;
  w->theme = T_GRASS; w->sky = S_DAY;
  w->nDecoTypes = 0;
  snprintf(w->name, sizeof(w->name), "%s", w->tracks.title[ti]);
  addSection(12, 0, 0);   // başlangıç düzlüğü
  struct Obj { int s; float x; Deco d; };
  Obj objs[24];
  int nObj = 0;
  TextReader r(txt);
  while (r.next()) {
    if (!r.section[0]) {
      const char *k, *v;
      if (!r.kv(&k, &v)) continue;
      if (streq(k, "laps")) w->laps = toInt(v, 3);
      else if (streq(k, "skill")) w->skill = toFloat(v, 0.9f);
      else if (streq(k, "density")) w->density = toFloat(v, 0.35f);
      else if (streq(k, "sky")) w->sky = streq(v, "dusk") ? S_DUSK : streq(v, "night") ? S_NIGHT : S_DAY;
      else if (streq(k, "theme"))
        w->theme = streq(v, "desert") ? T_DESERT : streq(v, "snow") ? T_SNOW : streq(v, "city") ? T_CITY : T_GRASS;
      else if (streq(k, "deco")) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%s", v);
        char* save = nullptr;
        for (char* tok = strtok_r(buf, ", ", &save); tok && w->nDecoTypes < 4; tok = strtok_r(nullptr, ", ", &save)) {
          Deco d = decoByName(tok);
          if (d) w->decoTypes[w->nDecoTypes++] = d;
        }
      }
    } else if (r.in("road")) {
      float f[3] = { 0, 0, 0 };
      int c = parseFloats(r.line, f, 3);
      if (c >= 1) {
        // viraj -8..8, yokuş ±3000 ile sınırlı
        float cv = f[1] < -8 ? -8 : f[1] > 8 ? 8 : f[1];
        float hl = f[2] < -3000 ? -3000 : f[2] > 3000 ? 3000 : f[2];
        addSection((int)f[0], cv, hl);
        // keskin virajdan önce dış tarafa yön tabelaları
        if (fabsf(cv) >= 3 && nObj < 24 - 2) {
          int start = w->n - (int)f[0];
          for (int k = 8; k >= 2 && nObj < 24; k -= 3)
            objs[nObj++] = { start - k, cv > 0 ? -1.4f : 1.4f, cv > 0 ? D_SIGN_R : D_SIGN_L };
        }
      }
    } else if (r.in("objects") && nObj < 24) {
      char* rest;
      const char* a = splitWord(r.line, &rest);
      char* rest2;
      const char* b = splitWord(rest, &rest2);
      Deco d = decoByName(rest2);
      if (d) objs[nObj++] = { toInt(a), toFloat(b), d };
    }
  }
  w->tracks.release(txt, owned);
  if (fabsf(w->lastY) > 1) addSection(30, 0, -w->lastY);   // döngü kapansın
  addSection(10, 0, 0);
  if (w->n < 40) return false;
  w->trackLen = w->n * SEG_L;
  if (w->nDecoTypes == 0) { w->decoTypes[0] = D_TREE; w->decoTypes[1] = D_BUSH; w->nDecoTypes = 2; }
  // rastgele ama pist adına göre sabit çevre objeleri
  uint32_t seed = 2166136261u;
  for (const char* p = w->name; *p; p++) { seed ^= (uint8_t)*p; seed *= 16777619u; }
  for (int i = 14; i < w->n - 2; i++) {
    uint32_t h = hash32(seed + i * 7919);
    if ((h & 1023) / 1024.0f > w->density) continue;
    float side = (h & 1024) ? 1.0f : -1.0f;
    float x = side * (1.45f + ((h >> 12) & 255) / 255.0f * 1.4f);
    placeDeco(i, (Deco)w->decoTypes[(h >> 20) % w->nDecoTypes], x);
  }
  if (w->sky == S_NIGHT)
    for (int i = 16; i < w->n; i += 12) placeDeco(i, D_LAMP, (i / 12) & 1 ? 1.3f : -1.3f);
  for (int i = 0; i < nObj; i++) if (objs[i].s >= 0) placeDeco(objs[i].s, objs[i].d, objs[i].x);
  setTheme();
  hwLog("Yaris: %s %d segment | heap %u", w->name, w->n, (unsigned)hwFreeHeap());
  return true;
}

// ---------- yarış ----------
static const uint16_t CAR_COLORS[NRIV] = {
  rgb(40, 90, 230), rgb(240, 200, 30), rgb(40, 200, 80), rgb(230, 120, 30), rgb(170, 60, 220),
};

static void startRace(int ti) {
  if (!buildTrack(ti)) { snprintf(w->msg, sizeof(w->msg), "PIST YUKLENEMEDI"); w->msgT = 2; return; }
  w->trackIdx = ti;
  w->dist = 0; w->speed = 0; w->px = 0; w->steer = 0;
  w->lap = 0; w->lapTime = w->raceTime = 0; w->bestLap = w->lastLap = 0;
  w->nitros = 3; w->nitroT = 0; w->crashT = 0; w->finished = false; w->msgT = 0;
  w->place = NRIV + 1;
  float diff = w->skill * (1.0f + ti * 0.03f);
  for (int k = 0; k < NRIV; k++) {
    Car& c = w->cars[k];
    c.dist = SEG_L * 2.5f * (k + 1);
    c.x = c.targetX = (k & 1) ? -0.5f : 0.5f;
    c.speed = 0;
    c.maxSpeed = MAXSPD * (0.78f + k * 0.035f) * diff;
    if (c.maxSpeed > MAXSPD * 0.97f) c.maxSpeed = MAXSPD * 0.97f;
    c.color = CAR_COLORS[k];
    c.laneT = 1 + frand() * 3;
    c.finished = false;
  }
  w->mode = M_COUNT;
  w->modeT = 0;
  input::clear();
}

static void finishRace() {
  w->finished = true;
  w->mode = M_RESULT;
  w->modeT = 0;
  int place = 1;
  for (const Car& c : w->cars) if (c.dist > w->dist) place++;
  w->place = place;
  char key[16];
  save::keyFor(key, "rcp", w->name);
  int prevBest = save::get(key, 99);
  if (place < prevBest) {
    save::set(key, place);
    if (place == 1) save::set("rc_win", save::get("rc_win", 0) + 1);
  }
  save::keyFor(key, "rcl", w->name);
  w->newBestLap = w->bestLap > 0 && save::setMin(key, (int32_t)(w->bestLap * 1000));
  w->unlockedNew = false;
  if (place <= 3 && w->trackIdx + 2 > w->unlocked && w->trackIdx + 1 < w->tracks.count) {
    w->unlocked = w->trackIdx + 2;
    save::set("rc_unl", w->unlocked);
    w->unlockedNew = true;
  }
  input::clear();
}

static void updateCars(float dt) {
  for (Car& c : w->cars) {
    int si = (int)(fmodf(c.dist, w->trackLen) / SEG_L);
    float cv = fabsf(curveOf(si));
    float target = c.maxSpeed * (1.0f - cv * 0.025f);
    float gap = c.dist - w->dist;                       // lastik bant: çok geride kalan hızlanır
    if (gap < -SEG_L * 30) target *= 1.08f;
    else if (gap > SEG_L * 40) target *= 0.94f;
    float acc = MAXSPD / 5;
    c.speed += (c.speed < target ? acc : -acc) * dt;
    if (w->mode != M_RACE && w->mode != M_RESULT) c.speed = 0;
    c.dist += c.speed * dt;
    // şerit değiştirme: oyuncu hemen arkadaysa yol ver, ara sıra rastgele
    c.laneT -= dt;
    if (gap > 0 && gap < SEG_L * 4 && fabsf(w->px - c.x) < 0.4f) c.targetX = w->px > 0 ? -0.6f : 0.6f;
    else if (c.laneT <= 0) { c.targetX = (frand() * 2 - 1) * 0.7f; c.laneT = 2 + frand() * 4; }
    c.x += (c.targetX - c.x) * (dt * 1.5f > 1 ? 1 : dt * 1.5f);
    if (!c.finished && c.dist >= w->laps * w->trackLen) c.finished = true;
  }
}

static void update(float dt) {
  w->modeT += dt;
  if (w->msgT > 0) w->msgT -= dt;
  switch (w->mode) {
    case M_SELECT: {
      int r = w->list.update(w->tracks.count, dt, 6);
      if (r == -2) app::requestMenu();
      else if (r >= 0) {
        if (r < w->unlocked) startRace(r);
        else { snprintf(w->msg, sizeof(w->msg), "ILK 3'E GIR, AC"); w->msgT = 1.5f; }
      }
      return;
    }
    case M_COUNT:
      if (w->modeT >= 3.0f) { w->mode = M_RACE; w->modeT = 0; }
      if (input::down(B_FWD)) w->speed = 0;   // erken çıkış yok
      return;
    case M_PAUSE:
      if (input::pressed(B_RELOAD) || input::pressed(B_FIRE)) { w->mode = M_RACE; input::clear(); }
      return;
    case M_RESULT:
      updateCars(dt);
      w->speed *= 1.0f - dt * 0.8f;
      w->dist += w->speed * dt;
      if (w->modeT > 1.0f && input::pressed(B_FIRE)) { w->mode = M_SELECT; input::clear(); }
      return;
    case M_RACE:
      break;
  }
  if (input::pressed(B_RELOAD)) { w->mode = M_PAUSE; return; }
  float pct = w->speed / MAXSPD;
  float dx = dt * 2.2f * pct;
  w->steer = 0;
  if (input::down(B_LEFT))  { w->px -= dx; w->steer = -1; }
  if (input::down(B_RIGHT)) { w->px += dx; w->steer = 1; }
  int ps = (int)(fmodf(w->dist, w->trackLen) / SEG_L);
  w->px -= dx * pct * curveOf(ps) * CENTRIFUGAL;
  w->skyOff += curveOf(ps) * pct * dt * 40;

  if (input::pressed(B_FIRE) && w->nitros > 0 && w->nitroT <= 0) { w->nitros--; w->nitroT = 2.5f; }
  if (w->nitroT > 0) w->nitroT -= dt;
  float maxS = MAXSPD * (w->nitroT > 0 ? 1.25f : 1.0f);
  if (input::down(B_FWD)) w->speed += ACCEL * (w->nitroT > 0 ? 1.8f : 1.0f) * dt;
  else if (input::down(B_BACK)) w->speed += BRAKE * dt;
  else w->speed += DECEL * dt;
  if (w->nitroT > 0) w->speed += ACCEL * 0.6f * dt;

  bool offroad = fabsf(w->px) > 1.0f;
  if (offroad && w->speed > OFF_LIMIT) w->speed += OFF_DECEL * dt;
  if (w->px < -2.6f) w->px = -2.6f;
  if (w->px > 2.6f) w->px = 2.6f;
  if (w->speed < 0) w->speed = 0;
  if (w->speed > maxS) w->speed = maxS;
  if (w->crashT > 0) w->crashT -= dt;

  // çevre objelerine çarpma
  if (offroad) {
    for (int k = 0; k < 2; k++) {
      const Seg& s = w->seg[idx(ps + k)];
      if (!s.deco || !DECOS[s.deco].solid) continue;
      float dw = DECOS[s.deco].w / ROAD_W * 0.5f;
      if (fabsf(w->px - s.decoX10 * 0.1f) < dw + 0.15f) {
        w->speed = MAXSPD / 8;
        w->px += w->px > 0 ? -0.15f : 0.15f;
        w->crashT = 0.4f;
        break;
      }
    }
  }
  // rakiplere çarpma
  for (Car& c : w->cars) {
    float gap = c.dist - w->dist;
    if (gap > 0 && gap < SEG_L * 0.9f && fabsf(w->px - c.x) < 0.42f && w->speed > c.speed) {
      w->speed = c.speed * 0.85f;
      w->dist = c.dist - SEG_L * 0.9f;
      w->crashT = 0.25f;
    }
  }

  int prevLap = (int)(w->dist / w->trackLen);
  w->dist += w->speed * dt;
  w->lapTime += dt;
  w->raceTime += dt;
  if ((int)(w->dist / w->trackLen) > prevLap) {
    w->lastLap = w->lapTime;
    if (w->bestLap == 0 || w->lapTime < w->bestLap) w->bestLap = w->lapTime;
    w->lapTime = 0;
    w->lap++;
    if (w->lap >= w->laps) { updateCars(dt); finishRace(); return; }
    snprintf(w->msg, sizeof(w->msg), w->lap == w->laps - 1 ? "SON TUR!" : "TUR %d", w->lap + 1);
    w->msgT = 1.5f;
  }
  updateCars(dt);
  w->place = 1;
  for (const Car& c : w->cars) if (c.dist > w->dist) w->place++;
}

// ---------- çizim: objeler ----------
static void drawCar(int cx, int bottom, int wd, uint16_t col, bool brake, float steer) {
  if (wd < 3) { gfx::fillRect(cx - 1, bottom - 1, 2, 1, col); return; }
  int h = wd / 2 + 1;
  int x0 = cx - wd / 2;
  int lean = (int)(steer * wd * 0.06f);
  gfx::fillRect(x0 + wd / 10, bottom - h / 5, wd / 5 + 1, h / 5 + 1, C_BLACK);          // tekerler
  gfx::fillRect(x0 + wd - wd / 10 - wd / 5 - 1, bottom - h / 5, wd / 5 + 1, h / 5 + 1, C_BLACK);
  gfx::fillRect(x0, bottom - h * 6 / 10, wd, h * 4 / 10, col);                            // gövde
  gfx::hline(x0, bottom - h * 6 / 10, wd, mix(col, C_WHITE, 90));
  gfx::fillRect(x0 + wd * 2 / 10 + lean, bottom - h, wd * 6 / 10, h * 4 / 10 + 1, shade(col, 170)); // kabin
  gfx::fillRect(x0 + wd * 3 / 10 + lean, bottom - h + h / 10 + 1, wd * 4 / 10, h * 2 / 10 + 1, rgb(30, 40, 70));
  uint16_t tl = brake ? rgb(255, 40, 40) : rgb(150, 0, 0);
  int lw = wd / 6 + 1, lh = h / 8 + 1;
  gfx::fillRect(x0 + 1, bottom - h * 5 / 10, lw, lh, tl);
  gfx::fillRect(x0 + wd - lw - 1, bottom - h * 5 / 10, lw, lh, tl);
  gfx::hline(x0, bottom - h / 5 - 1, wd, shade(col, 110));
}

static void drawDeco(Deco d, int cx, int bottom, int wd, int ht) {
  if (wd < 1 || ht < 1) return;
  bool night = w->sky == S_NIGHT;
  int f = night ? 140 : 256;
  switch (d) {
    case D_TREE: {
      gfx::fillRect(cx - wd / 12, bottom - ht * 4 / 10, wd / 6 + 1, ht * 4 / 10, shade(rgb(100, 60, 30), f));
      int r = wd / 2;
      gfx::fillCircle(cx, bottom - ht + r, r, shade(rgb(30, 110, 40), f));
      gfx::fillCircle(cx - r / 3, bottom - ht + r - r / 4, r * 2 / 3, shade(rgb(50, 150, 55), f));
      break;
    }
    case D_PINE: {
      gfx::fillRect(cx - wd / 14, bottom - ht / 5, wd / 7 + 1, ht / 5, shade(rgb(90, 55, 30), f));
      uint16_t g = shade(rgb(25, 90, 50), f);
      for (int i = 0; i < 3; i++) {
        int ty = bottom - ht + i * ht / 4, by = ty + ht * 4 / 10, hw = wd * (2 + i) / 8;
        gfx::fillTriangle(cx, ty, cx - hw, by, cx + hw, by, g);
        if (w->theme == T_SNOW) gfx::fillTriangle(cx, ty, cx - hw / 3, ty + ht / 12, cx + hw / 3, ty + ht / 12, C_WHITE);
      }
      break;
    }
    case D_PALM: {
      uint16_t tr = shade(rgb(130, 90, 50), f);
      int t = wd / 10 + 1;
      for (int i = 0; i < 6; i++) {
        int y0 = bottom - ht * i / 6, y1 = bottom - ht * (i + 1) / 6;
        int x0 = cx + (int)(sinf(i * 0.4f) * wd * 0.15f), x1 = cx + (int)(sinf((i + 1) * 0.4f) * wd * 0.15f);
        gfx::thickLine(x0, y0, x1, y1, t, tr);
      }
      int tx = cx + (int)(sinf(2.4f) * wd * 0.15f), ty = bottom - ht;
      uint16_t lf = shade(rgb(40, 160, 60), f);
      for (int i = -2; i <= 2; i++)
        gfx::thickLine(tx, ty, tx + i * wd / 4, ty + abs(i) * ht / 10 + ht / 12, t, lf);
      break;
    }
    case D_CACTUS: {
      uint16_t g = shade(rgb(50, 150, 70), f);
      int t = wd / 3 + 1;
      gfx::fillRect(cx - t / 2, bottom - ht, t, ht, g);
      gfx::fillRect(cx - wd / 2, bottom - ht * 6 / 10, t * 2 / 3 + 1, ht / 4, g);
      gfx::fillRect(cx - wd / 2, bottom - ht * 6 / 10, wd / 2, t / 2 + 1, g);
      gfx::fillRect(cx + wd / 2 - t * 2 / 3, bottom - ht * 8 / 10, t * 2 / 3 + 1, ht / 4, g);
      gfx::fillRect(cx, bottom - ht * 6 / 10, wd / 2, t / 2 + 1, g);
      break;
    }
    case D_ROCK:
      gfx::fillEllipse(cx, bottom - ht / 2, wd / 2, ht / 2, shade(rgb(130, 120, 110), f));
      gfx::fillEllipse(cx - wd / 8, bottom - ht * 6 / 10, wd / 4, ht / 4, shade(rgb(170, 160, 150), f));
      break;
    case D_BUSH:
      gfx::fillEllipse(cx, bottom - ht / 2, wd / 2, ht / 2, shade(rgb(40, 120, 45), f));
      gfx::fillEllipse(cx - wd / 6, bottom - ht * 6 / 10, wd / 4, ht / 4, shade(rgb(70, 160, 70), f));
      break;
    case D_SIGN_L:
    case D_SIGN_R: {
      gfx::fillRect(cx - 1, bottom - ht / 2, wd / 10 + 1, ht / 2, rgb(120, 120, 120));
      int bh = ht / 2, by = bottom - ht;
      gfx::fillRect(cx - wd / 2, by, wd, bh, rgb(250, 210, 30));
      gfx::rect(cx - wd / 2, by, wd, bh, C_BLACK);
      int dir = d == D_SIGN_R ? 1 : -1;
      gfx::fillTriangle(cx + dir * wd * 4 / 10, by + bh / 2, cx - dir * wd / 10, by + bh / 6, cx - dir * wd / 10,
                        by + bh * 5 / 6, rgb(200, 20, 20));
      gfx::fillRect(dir > 0 ? cx - wd * 3 / 10 : cx - wd / 10, by + bh * 4 / 10, wd * 4 / 10, bh / 5 + 1, rgb(200, 20, 20));
      break;
    }
    case D_POST:
      gfx::fillRect(cx - wd / 2, bottom - ht, wd + 1, ht, rgb(230, 230, 230));
      gfx::fillRect(cx - wd / 2, bottom - ht, wd + 1, ht / 5 + 1, rgb(220, 30, 30));
      break;
    case D_BILLBOARD: {
      gfx::fillRect(cx - wd * 4 / 10, bottom - ht / 2, wd / 14 + 1, ht / 2, rgb(80, 80, 80));
      gfx::fillRect(cx + wd * 3 / 10, bottom - ht / 2, wd / 14 + 1, ht / 2, rgb(80, 80, 80));
      int by = bottom - ht, bh = ht * 55 / 100;
      gfx::fillRect(cx - wd / 2, by, wd, bh, shade(rgb(200, 30, 40), f));
      gfx::fillRect(cx - wd / 2, by + bh * 7 / 10, wd, bh * 3 / 10, shade(rgb(250, 220, 60), f));
      if (wd >= 36) gfx::text(cx, by + bh / 4, "GECE", C_WHITE, wd >= 70 ? 2 : 1, AL_CENTER);
      break;
    }
    case D_LAMP: {
      gfx::fillRect(cx - wd / 4, bottom - ht, wd / 2 + 1, ht, rgb(90, 90, 100));
      int r = wd / 2 + 1;
      if (night) gfx::fillCircle(cx, bottom - ht, r * 2, rgb(120, 110, 50));
      gfx::fillCircle(cx, bottom - ht, r, rgb(255, 240, 160));
      break;
    }
    case D_HOUSE: {
      int bh = ht * 6 / 10;
      gfx::fillRect(cx - wd / 2, bottom - bh, wd, bh, shade(rgb(210, 190, 160), f));
      gfx::fillTriangle(cx - wd / 2 - wd / 10, bottom - bh, cx + wd / 2 + wd / 10, bottom - bh, cx, bottom - ht,
                        shade(rgb(170, 60, 40), f));
      uint16_t win = night ? rgb(255, 220, 100) : rgb(80, 120, 170);
      gfx::fillRect(cx - wd * 3 / 10, bottom - bh * 7 / 10, wd / 5 + 1, bh / 4 + 1, win);
      gfx::fillRect(cx + wd / 10, bottom - bh * 7 / 10, wd / 5 + 1, bh / 4 + 1, win);
      break;
    }
    default: break;
  }
}

// ---------- çizim: yol ----------
static void drawBackground(int horizon) {
  const Theme& t = w->th;
  gfx::vgradient(0, 0, SCR_W, horizon + 12, t.skyTop, t.skyBot);
  if (w->sky == S_NIGHT) {
    for (int i = 0; i < 30; i++) {
      int sx = ((int)(hash32(i * 31) % 512 - w->skyOff * 0.3f) % SCR_W + SCR_W) % SCR_W;
      gfx::pixel(sx, (int)(hash32(i * 17 + 3) % (horizon > 10 ? horizon - 10 : 1)), C_WHITE);
    }
    gfx::fillCircle(100, 16, 6, rgb(240, 240, 220));
  } else if (w->sky == S_DUSK) {
    gfx::fillCircle(40 - (int)(w->skyOff * 0.1f) % 160 + 60, horizon - 6, 12, rgb(255, 200, 90));
  }
  // iki katman tepe / şehir silueti, virajda kayar
  for (int x = 0; x < SCR_W; x++) {
    float a = (x + w->skyOff * 0.5f) * 0.05f;
    int h1 = (int)(10 + sinf(a) * 5 + sinf(a * 2.7f + 1) * 3);
    float b = (x + w->skyOff) * 0.09f;
    int h2 = (int)(6 + sinf(b) * 4 + sinf(b * 3.1f) * 2);
    if (w->theme == T_CITY) {
      int bx = (int)(x + w->skyOff * 0.5f);
      h1 = 6 + (int)(hash32((uint32_t)(bx / 9) + 77) % 22);
      gfx::vline(x, horizon - h1, h1 + 12, t.hill);
      if (((bx % 9) == 3 || (bx % 9) == 6) && (hash32((uint32_t)bx * 3) & 3) == 0)
        gfx::pixel(x, horizon - h1 + 3, rgb(255, 220, 90));
      continue;
    }
    gfx::vline(x, horizon - h1, h1 + 12, shade(t.hill, 180));
    gfx::vline(x, horizon - h2, h2 + 12, t.hill);
  }
}

static inline uint16_t fogged(uint16_t c, int k) {
  int f = k * 256 / DRAW;
  return mix(c, w->th.fog, f * f / 256 * 3 / 4);
}

static void drawRoadRows(int k, int si, int clip) {
  float xn = w->ex[k], yn = w->ey[k], wn = w->ew[k];
  float xf = w->ex[k + 1], yf = w->ey[k + 1], wf = w->ew[k + 1];
  int ya = (int)ceilf(yf), yb = (int)ceilf(yn);
  if (ya < 0) ya = 0;
  if (yb > clip) yb = clip;
  if (ya >= yb) return;
  bool alt = (si / 3) & 1;
  const Theme& t = w->th;
  uint16_t grass = fogged(alt ? t.grass1 : t.grass2, k);
  uint16_t rumble = fogged(alt ? t.rumble1 : t.rumble2, k);
  uint16_t road = fogged(alt ? t.road1 : t.road2, k);
  uint16_t lane = fogged(t.lane, k);
  bool start = si < 2;
  float inv = 1.0f / (yn - yf);
  for (int y = ya; y < yb; y++) {
    float p = (y - yf) * inv;
    float cx = xf + (xn - xf) * p, rw = wf + (wn - wf) * p;
    int l = (int)(cx - rw), r = (int)(cx + rw);
    int rm = (int)(rw * 0.12f) + 1;
    gfx::hline(0, y, SCR_W, grass);
    gfx::hline(l - rm, y, rm, rumble);
    gfx::hline(r, y, rm, rumble);
    if (start) {   // damalı başlangıç çizgisi
      int sq = (int)(rw / 4) + 1;
      for (int x = l, i = 0; x < r; x += sq, i++)
        gfx::hline(x, y, x + sq > r ? r - x : sq, ((i + (y / 2)) & 1) ? C_WHITE : C_BLACK);
    } else {
      gfx::hline(l, y, r - l, road);
      if (!alt) {
        int lw = (int)(rw * 0.03f) + 1;
        gfx::hline((int)(cx - rw / 3) - lw / 2, y, lw, lane);
        gfx::hline((int)(cx + rw / 3) - lw / 2, y, lw, lane);
      }
    }
  }
}

static void renderRoad() {
  float L = w->trackLen;
  float cam = fmodf(w->dist - PLAYER_Z + L * 4, L);
  int base = (int)(cam / SEG_L);
  float basePct = (cam - base * SEG_L) / SEG_L;
  float pz = fmodf(w->dist, L);
  int pi = (int)(pz / SEG_L);
  float pp = (pz - pi * SEG_L) / SEG_L;
  float camY = yOf(pi) + (yOf(pi + 1) - yOf(pi)) * pp + CAM_H;

  float x = 0, dx = -curveOf(base) * basePct;
  for (int k = 0; k <= DRAW; k++) {
    float dz = (k - basePct) * SEG_L;
    float wy = yOf(base + k);
    w->evis[k] = dz > 1.0f;
    float s = w->evis[k] ? CAM_D / dz : 0;
    w->es[k] = s;
    w->ex[k] = HALF + s * (x - w->px * ROAD_W) * HALF;
    w->ey[k] = HALF - s * (wy - camY) * HALF;
    w->ew[k] = s * ROAD_W * HALF;
    x += dx;
    dx += curveOf(base + k);
  }
  // ufuk: en uzak görünen kenar
  int horizon = HALF;
  for (int k = DRAW; k > 0; k--) if (w->evis[k]) { horizon = (int)w->ey[k]; break; }
  if (horizon < 20) horizon = 20;
  if (horizon > 100) horizon = 100;
  drawBackground(horizon);

  int maxy = SCR_H;
  for (int k = 0; k < DRAW; k++) {
    w->eclip[k] = maxy;
    if (!w->evis[k] || !w->evis[k + 1] || w->ey[k + 1] >= w->ey[k] || w->ey[k + 1] >= maxy) continue;
    drawRoadRows(k, idx(base + k), maxy);
    maxy = (int)ceilf(w->ey[k + 1]);
  }

  // sprite'lar uzaktan yakına
  for (int k = DRAW - 1; k >= 1; k--) {
    if (!w->evis[k]) continue;
    int si = idx(base + k);
    const Seg& sg = w->seg[si];
    gfx::setClipBottom(w->eclip[k]);
    float s = w->es[k];
    if (sg.deco) {
      const DecoInfo& di = DECOS[sg.deco];
      int sx = (int)(w->ex[k] + s * sg.decoX10 * 0.1f * ROAD_W * HALF);
      int wd = (int)(s * di.w * HALF), ht = (int)(s * di.h * HALF);
      if (sx + wd > -10 && sx - wd < SCR_W + 10) drawDeco((Deco)sg.deco, sx, (int)w->ey[k], wd, ht);
    }
    for (const Car& c : w->cars) {
      float rel = fmodf(c.dist - cam + L * 8, L);
      int ck = (int)(rel / SEG_L);
      if (ck != k) continue;
      float fr = rel / SEG_L - ck;
      float cs = s + (w->es[k + 1] - s) * fr;
      float cx = w->ex[k] + (w->ex[k + 1] - w->ex[k]) * fr + cs * c.x * ROAD_W * HALF;
      float cy = w->ey[k] + (w->ey[k + 1] - w->ey[k]) * fr;
      drawCar((int)cx, (int)cy, (int)(cs * 620 * HALF), fogged(c.color, k), false, 0);
    }
    if (si == 1 && s > 0) {   // başlangıç/bitiş kapısı
      int lx = (int)(w->ex[k] - w->ew[k] * 1.15f), rx = (int)(w->ex[k] + w->ew[k] * 1.15f);
      int top = (int)(w->ey[k] - s * 1400 * HALF), bh = (int)(s * 260 * HALF) + 1;
      gfx::fillRect(lx, top, (int)(s * 80 * HALF) + 1, (int)w->ey[k] - top, rgb(90, 90, 100));
      gfx::fillRect(rx, top, (int)(s * 80 * HALF) + 1, (int)w->ey[k] - top, rgb(90, 90, 100));
      gfx::fillRect(lx, top, rx - lx, bh, rgb(30, 30, 30));
      if (bh >= 9) gfx::text((lx + rx) / 2, top + (bh - 7) / 2, "BITIS", C_WHITE, 1, AL_CENTER);
    }
  }
  gfx::setClipBottom(SCR_H);
}

static void renderHUD() {
  gfx::darken(0, 0, SCR_W, 11, 120);
  gfx::textf(2, 2, w->place == 1 ? rgb(255, 210, 40) : C_WHITE, 1, AL_LEFT, "%d/%d", w->place, NRIV + 1);
  gfx::textf(SCR_W / 2 - 8, 2, C_WHITE, 1, AL_CENTER, "T%d/%d", w->lap + 1 > w->laps ? w->laps : w->lap + 1, w->laps);
  char tb[16];
  ui::formatTime(tb, sizeof(tb), (uint32_t)(w->lapTime * 1000));
  gfx::text(SCR_W - 2, 2, tb, rgb(200, 230, 255), 1, AL_RIGHT);
  int kmh = (int)(w->speed / MAXSPD * 240);
  gfx::textf(SCR_W - 2, 118, C_WHITE, 1, AL_RIGHT, "%d", kmh);
  gfx::text(SCR_W - 2, 108, "km/sa", C_GREY, 1, AL_RIGHT);
  for (int i = 0; i < 3; i++) {
    uint16_t c = i < w->nitros ? rgb(60, 200, 255) : rgb(40, 50, 60);
    gfx::fillRect(3 + i * 7, 117, 5, 8, c);
  }
  if (w->nitroT > 0) ui::bar(3, 113, 19, 2, w->nitroT / 2.5f, rgb(60, 200, 255), C_BLACK);
  if (w->msgT > 0) gfx::textShadow(SCR_W / 2, 24, w->msg, rgb(255, 220, 60), 2, AL_CENTER);
}

static void renderPlayer() {
  int bob = (fabsf(w->px) > 1 && w->speed > 100) ? (int)(hash32((uint32_t)(w->raceTime * 30)) & 1) : 0;
  if (w->crashT > 0) bob += (int)(hash32((uint32_t)(w->raceTime * 40)) % 3) - 1;
  int cx = SCR_W / 2 + (int)(w->steer * 3);
  int bottom = SCR_H - 4 + bob;
  if (w->nitroT > 0) {
    int fl = 3 + (int)(hash32((uint32_t)(w->raceTime * 25)) % 4);
    gfx::fillCircle(cx - 10, bottom + 1, fl, rgb(80, 180, 255));
    gfx::fillCircle(cx + 10, bottom + 1, fl, rgb(80, 180, 255));
  }
  drawCar(cx, bottom, 46, rgb(220, 30, 30), input::down(B_BACK) && w->mode == M_RACE, w->steer);
  if (fabsf(w->px) > 1 && w->speed > 500) {   // toz
    for (int i = 0; i < 4; i++) {
      int dx = (int)(hash32((uint32_t)(w->raceTime * 20) + i) % 40) - 20;
      gfx::fillCircle(cx + dx, bottom - 2, 2, w->th.grass1);
    }
  }
}

static void selectRow(int i, int x, int y, int wd, int h, bool sel, float t) {
  (void)t;
  bool locked = i >= w->unlocked;
  if (sel) gfx::fillRect(x, y, wd - 4, h - 1, rgb(20, 40, 90));
  char b[32];
  snprintf(b, sizeof(b), "%d %.13s", i + 1, w->tracks.title[i]);
  gfx::text(x + 3, y + 2, b, locked ? rgb(80, 80, 100) : (sel ? C_WHITE : rgb(180, 190, 220)));
  char key[16];
  save::keyFor(key, "rcp", w->tracks.title[i]);
  int best = save::get(key, 0);
  if (locked) {
    gfx::fillRect(x + wd - 13, y + 5, 7, 5, rgb(120, 110, 60));
    gfx::rect(x + wd - 12, y + 2, 5, 4, rgb(120, 110, 60));
  } else if (best == 1) {
    gfx::fillRect(x + wd - 14, y + 2, 7, 4, rgb(255, 200, 40));   // kupa
    gfx::fillRect(x + wd - 12, y + 6, 3, 2, rgb(255, 200, 40));
    gfx::fillRect(x + wd - 14, y + 8, 7, 2, rgb(200, 150, 30));
  } else if (best > 0) {
    gfx::textf(x + wd - 8, y + 2, rgb(200, 200, 120), 1, AL_RIGHT, "%d.", best);
  }
}

static void renderSelect() {
  gfx::vgradient(0, 0, SCR_W, SCR_H, rgb(10, 20, 60), rgb(0, 0, 10));
  for (int i = 0; i < 6; i++) {   // hareketli yol çizgileri
    int y = (int)fmodf(w->modeT * 60 + i * 22, 132) - 4;
    gfx::fillRect(SCR_W - 6, y, 3, 10, rgb(40, 60, 120));
  }
  gfx::textShadow(SCR_W / 2, 4, "YARIS", rgb(255, 210, 40), 2, AL_CENTER);
  gfx::text(SCR_W / 2, 22, "KARIYER", C_GREY, 1, AL_CENTER);
  ui::listDraw(w->list, w->tracks.count, 2, 34, SCR_W - 8, 12, 6, selectRow, w->modeT);
  gfx::fillRect(0, 108, SCR_W, 20, C_BLACK);
  if (w->list.sel < w->tracks.count) {
    char key[16], tb[16];
    save::keyFor(key, "rcl", w->tracks.title[w->list.sel]);
    int32_t bl = save::get(key, -1);
    if (bl >= 0) { ui::formatTime(tb, sizeof(tb), (uint32_t)bl); gfx::textf(SCR_W / 2, 110, rgb(150, 200, 255), 1, AL_CENTER, "En iyi tur %s", tb); }
    else gfx::text(SCR_W / 2, 110, "Ilk 3 = sonraki pist", rgb(150, 160, 200), 1, AL_CENTER);
  }
  gfx::textf(SCR_W / 2, 120, rgb(100, 110, 140), 1, AL_CENTER, sd::ok() ? "SD: %d ek pist" : "SD yok: %d pist",
             sd::ok() ? w->tracks.count - w->tracks.builtinCount : w->tracks.count);
  if (w->msgT > 0) gfx::textShadow(SCR_W / 2, 96, w->msg, rgb(255, 120, 80), 1, AL_CENTER);
}

static void render() {
  if (w->mode == M_SELECT) { renderSelect(); return; }
  renderRoad();
  renderPlayer();
  renderHUD();
  if (w->mode == M_COUNT) {
    int c = 3 - (int)w->modeT;
    char b[12];
    snprintf(b, sizeof(b), "%d", c);
    gfx::textShadow(SCR_W / 2, 40, b, rgb(255, 220, 60), 4, AL_CENTER);
    gfx::text(SCR_W / 2, 74, w->name, C_WHITE, 1, AL_CENTER);
  } else if (w->mode == M_RACE && w->modeT < 0.8f) {
    gfx::textShadow(SCR_W / 2, 44, "BASLA!", rgb(80, 255, 80), 2, AL_CENTER);
  } else if (w->mode == M_PAUSE) {
    ui::banner("DURDU", "RELOAD: devam", C_WHITE);
  } else if (w->mode == M_RESULT) {
    ui::panel(10, 18, SCR_W - 20, 92, rgb(60, 120, 220), rgb(5, 10, 30));
    gfx::textShadow(SCR_W / 2, 24, "YARIS BITTI", C_WHITE, 1, AL_CENTER);
    char b[16];
    snprintf(b, sizeof(b), "%d.", w->place);
    gfx::textShadow(SCR_W / 2, 36, b, w->place == 1 ? rgb(255, 210, 40) : w->place <= 3 ? rgb(200, 220, 255) : rgb(200, 120, 100), 3, AL_CENTER);
    char tb[16];
    ui::formatTime(tb, sizeof(tb), (uint32_t)(w->raceTime * 1000));
    gfx::text(16, 64, "Sure", C_GREY);
    gfx::text(SCR_W - 16, 64, tb, C_WHITE, 1, AL_RIGHT);
    ui::formatTime(tb, sizeof(tb), (uint32_t)(w->bestLap * 1000));
    gfx::text(16, 75, "En iyi", C_GREY);
    gfx::text(SCR_W - 16, 75, tb, w->newBestLap ? rgb(255, 210, 40) : C_WHITE, 1, AL_RIGHT);
    if (w->unlockedNew) gfx::text(SCR_W / 2, 88, "YENI PIST ACILDI!", rgb(80, 255, 120), 1, AL_CENTER);
    else if (w->place > 3) gfx::text(SCR_W / 2, 88, "Ilk 3'e girmelisin", rgb(255, 140, 100), 1, AL_CENTER);
    if (w->modeT > 1.0f) gfx::text(SCR_W / 2, 99, "ATES: devam", C_GREY, 1, AL_CENTER);
  }
}

// ---------- Game arayüzü ----------
static bool enter() {
  void* m = calloc(1, sizeof(World));
  if (!m) return false;
  w = new (m) World;
  w->tracks.build(BUILTIN_TRACKS, BUILTIN_COUNT, "/race", ".trk");
  w->unlocked = save::get("rc_unl", 1);
  if (w->unlocked > w->tracks.count) w->unlocked = w->tracks.count;
  if (w->unlocked < 1) w->unlocked = 1;
  w->list.sel = w->unlocked - 1;
  w->mode = M_SELECT;
  hwLog("Yaris: World %u B, %d pist", (unsigned)sizeof(World), w->tracks.count);
  return true;
}

static void exitGame() {
  if (w) { w->~World(); free(w); }
  w = nullptr;
}

static void icon(int x, int y, float t) {
  gfx::vgradient(x, y, 24, 10, rgb(50, 120, 220), rgb(170, 210, 250));
  gfx::fillRect(x, y + 10, 24, 14, rgb(60, 150, 40));
  gfx::fillTriangle(x + 10, y + 10, x + 14, y + 10, x + 22, y + 23, rgb(100, 100, 105));
  gfx::fillTriangle(x + 10, y + 10, x + 2, y + 23, x + 22, y + 23, rgb(100, 100, 105));
  int off = t > 0 ? ((int)(t * 12) % 4) : 0;
  for (int i = 0; i < 3; i++) gfx::vline(x + 12, y + 11 + i * 5 + off, 2, C_WHITE);
  drawCar(x + 12, y + 23, 12, rgb(220, 30, 30), false, 0);
}

static void scoreText(char* buf, int n) {
  int total = BUILTIN_COUNT + sd::count("/race", ".trk");
  int u = save::get("rc_unl", 1);
  if (u > total) u = total;
  snprintf(buf, n, "Pist %d/%d  Kupa %d", u, total, (int)save::get("rc_win", 0));
}

static const char* const CONTROLS[] = {
  "ILERI", "Gaz",
  "GERI", "Fren",
  "SOL/SAG", "Direksiyon",
  "ATES", "Nitro (3)",
  "RELOAD", "Duraklat",
  nullptr, nullptr,
};

}  // namespace race

extern const Game RACE_GAME = {
  "YARIS", race::icon, race::scoreText, race::CONTROLS,
  race::enter, race::update, race::render, race::exitGame,
};
