// MOTOKROS — yandan görünüm, fizik tabanlı: iki yaylı teker, arka teker çekişi,
// öne/arkaya eğilme, takla, yanlış inişte düşme, kontrol noktaları, süre ve madalya.
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

namespace moto {

static const int   MAX_PTS = 700, MAX_OBJ = 40;
static const float PPM = 12.0f;             // piksel / metre
static const int   ORG_X = 48, ORG_Y = 74;  // bisikletin ekrandaki yeri
static const float G = 18.0f, MASS = 1.0f, INERTIA = 0.32f;
static const float WHEEL_R = 0.32f, SPRING = 650.0f, DAMP = 32.0f, MU = 1.3f;
static const float DRIVE = 24.0f, BRAKE = 30.0f, REVERSE = 6.0f, DRAG = 0.055f;
static const float PI_F = 3.14159265f;
static const float WOFF[2][2] = { { -0.62f, -0.36f }, { 0.62f, -0.36f } };   // arka, ön teker (gövde ekseninde)

enum ObjType : uint8_t { O_CHECKPOINT, O_FLAG, O_TREE, O_CACTUS, O_ROCK, O_SIGN, O_TIRE, O_COUNT };
static const char* const OBJ_NAMES[O_COUNT] = { "checkpoint", "flag", "tree", "cactus", "rock", "sign", "tire" };
enum ThemeId : uint8_t { TH_FOREST, TH_DESERT, TH_NIGHT, TH_SNOW };
enum Mode : uint8_t { M_SELECT, M_COUNT, M_RIDE, M_CRASH, M_PAUSE, M_FINISH };

struct Pt { float x, y; };
struct Obj { float x; uint8_t type; bool passed; };

struct World {
  LevelSet tracks;
  ui::List list;
  int unlocked, trackIdx;
  Mode mode;
  float modeT;

  Pt pts[MAX_PTS];
  int nPts;
  Obj objs[MAX_OBJ];
  int nObj;
  float finishX, gold, silver, bronze;
  uint8_t theme;
  char name[20];

  // bisiklet
  float x, y, vx, vy, a, w, spin[2];
  bool contact[2], wasAir;
  float airRot, airT, turboT, cpX, time, bonus, camX, camY, msgT, stuckT;
  int turbos, flips, cpIdx;
  char msg[24];

  // sonuç
  int medal;
  bool newBest, unlockedNew;
};
static World* w = nullptr;

// ---------- arazi ----------
static float groundAt(float x, float* slope = nullptr) {
  const Pt* p = w->pts;
  int n = w->nPts;
  if (x <= p[0].x) { if (slope) *slope = 0; return p[0].y; }
  if (x >= p[n - 1].x) { if (slope) *slope = 0; return p[n - 1].y; }
  int lo = 0, hi = n - 1;
  while (hi - lo > 1) {
    int mid = (lo + hi) / 2;
    if (p[mid].x <= x) lo = mid; else hi = mid;
  }
  float s = (p[hi].y - p[lo].y) / (p[hi].x - p[lo].x);
  if (slope) *slope = s;
  return p[lo].y + (x - p[lo].x) * s;
}

static void addPt(float x, float y) {
  if (w->nPts >= MAX_PTS) return;
  if (w->nPts && x <= w->pts[w->nPts - 1].x + 0.05f) x = w->pts[w->nPts - 1].x + 0.05f;
  w->pts[w->nPts++] = { x, y };
}

static void terrainCmd(const char* cmd, const float* f, int nf) {
  float x = w->pts[w->nPts - 1].x, y = w->pts[w->nPts - 1].y;
  float L = nf > 0 ? f[0] : 10, H = nf > 1 ? f[1] : 2;
  if (L < 1) L = 1;
  if (L > 200) L = 200;
  if (H > 20) H = 20;
  if (H < -20) H = -20;
  if (streq(cmd, "flat")) addPt(x + L, y);
  else if (streq(cmd, "up")) addPt(x + L, y + H);
  else if (streq(cmd, "down")) addPt(x + L, y - H);
  else if (streq(cmd, "pt")) addPt(x + L, y + H);
  else if (streq(cmd, "hill") || streq(cmd, "valley")) {
    float s = streq(cmd, "hill") ? 1.0f : -1.0f;
    for (int i = 1; i <= 12; i++) {
      float t = i / 12.0f;
      addPt(x + L * t, y + s * H * (1 - cosf(2 * PI_F * t)) / 2);
    }
  } else if (streq(cmd, "bumps")) {
    int n = nf > 2 ? (int)f[2] : 4;
    if (n < 1) n = 1;
    if (n > 20) n = 20;
    float bl = L / n;
    for (int b = 0; b < n; b++)
      for (int i = 1; i <= 6; i++) {
        float t = i / 6.0f;
        addPt(x + bl * (b + t), y + H * (1 - cosf(2 * PI_F * t)) / 2);
      }
  } else if (streq(cmd, "ramp")) {   // içbükey rampa, sonra sert iniş
    for (int i = 1; i <= 8; i++) {
      float t = i / 8.0f;
      addPt(x + L * t, y + H * t * t);
    }
    addPt(x + L + H * 0.5f + 0.5f, y);
  } else if (streq(cmd, "gap")) {    // çukur: önünde küçük sıçrama rampası, üzerinden atla
    for (int i = 1; i <= 4; i++) {
      float t = i / 4.0f;
      addPt(x + 3.0f * t, y + 0.8f * t * t);
    }
    addPt(x + 3.6f, y - H);
    addPt(x + 3.6f + L, y - H);
    addPt(x + 4.4f + L, y);
  }
}

static bool buildTrack(int ti) {
  bool owned;
  const char* txt = w->tracks.load(ti, &owned);
  if (!txt) return false;
  w->nPts = 0;
  w->nObj = 0;
  w->gold = w->silver = w->bronze = 0;
  w->theme = TH_FOREST;
  snprintf(w->name, sizeof(w->name), "%s", w->tracks.title[ti]);
  addPt(-30, 0);
  addPt(0, 0);
  TextReader r(txt);
  while (r.next()) {
    if (!r.section[0]) {
      const char *k, *v;
      if (!r.kv(&k, &v)) continue;
      if (streq(k, "gold")) w->gold = toFloat(v);
      else if (streq(k, "silver")) w->silver = toFloat(v);
      else if (streq(k, "bronze")) w->bronze = toFloat(v);
      else if (streq(k, "theme"))
        w->theme = streq(v, "desert") ? TH_DESERT : streq(v, "night") ? TH_NIGHT : streq(v, "snow") ? TH_SNOW : TH_FOREST;
    } else if (r.in("terrain")) {
      char* rest;
      const char* cmd = splitWord(r.line, &rest);
      float f[3];
      int nf = parseFloats(rest, f, 3);
      terrainCmd(cmd, f, nf);
    } else if (r.in("objects") && w->nObj < MAX_OBJ) {
      char* rest;
      const char* xs = splitWord(r.line, &rest);
      for (int t = 0; t < O_COUNT; t++)
        if (streq(rest, OBJ_NAMES[t])) { w->objs[w->nObj++] = { toFloat(xs), (uint8_t)t, false }; break; }
    }
  }
  w->tracks.release(txt, owned);
  float endX = w->pts[w->nPts - 1].x;
  const float tail[1] = { 40.0f };
  terrainCmd("flat", tail, 1);
  w->finishX = endX + 6;
  if (w->nPts < 4 || endX < 30) return false;
  // madalya süreleri verilmemişse uzunluktan tahmin et
  if (w->gold <= 0) w->gold = endX / 13.0f;
  if (w->silver <= w->gold) w->silver = w->gold * 1.25f;
  if (w->bronze <= w->silver) w->bronze = w->silver * 1.3f;
  // kontrol noktalarını x'e göre sırala
  for (int i = 1; i < w->nObj; i++) {
    Obj o = w->objs[i];
    int j = i - 1;
    while (j >= 0 && w->objs[j].x > o.x) { w->objs[j + 1] = w->objs[j]; j--; }
    w->objs[j + 1] = o;
  }
  hwLog("Motokros: %s %d nokta %.0f m", w->name, w->nPts, (double)endX);
  return true;
}

// ---------- bisiklet ----------
static void placeBike(float x) {
  float sl;
  float gy = groundAt(x, &sl);
  w->x = x;
  w->y = gy + 0.75f;
  w->a = atanf(sl);
  w->vx = w->vy = w->w = 0;
  w->airRot = 0;
  w->wasAir = false;
  w->stuckT = 0;
}

// Kontrol noktasından hareketli kalkış
static void respawn() {
  placeBike(w->cpX);
  if (w->cpX > 3) { w->vx = 6.0f * cosf(w->a); w->vy = 6.0f * sinf(w->a); }
}

static void startRun(int ti) {
  if (!buildTrack(ti)) { snprintf(w->msg, sizeof(w->msg), "PARKUR HATALI"); w->msgT = 2; return; }
  w->trackIdx = ti;
  placeBike(2);
  w->cpX = 2;
  w->cpIdx = 0;
  w->time = w->bonus = 0;
  w->turbos = 3;
  w->turboT = 0;
  w->flips = 0;
  w->camX = w->x;
  w->camY = w->y;
  w->msgT = 0;
  for (int i = 0; i < w->nObj; i++) w->objs[i].passed = false;
  w->mode = M_COUNT;
  w->modeT = 0;
  input::clear();
}

static inline void rot(float a, float x, float y, float& ox, float& oy) {
  float c = cosf(a), s = sinf(a);
  ox = x * c - y * s;
  oy = x * s + y * c;
}

static void crash() {
  hwLog("Motokros: dustu x=%.1f", (double)w->x);
  w->mode = M_CRASH;
  w->modeT = 0;
  snprintf(w->msg, sizeof(w->msg), "DUSTUN!");
  w->msgT = 1.4f;
}

static void physicsStep(float h, bool throttle, bool brake, float lean, bool control) {
  float fx = 0, fy = -G * MASS, tq = 0;
  bool air = true;
  for (int i = 0; i < 2; i++) {
    float ox, oy;
    rot(w->a, WOFF[i][0], WOFF[i][1], ox, oy);
    float cx = w->x + ox, cy = w->y + oy;
    float sl;
    float gy = groundAt(cx, &sl);
    float nl = 1.0f / sqrtf(1 + sl * sl);
    float nx = -sl * nl, ny = nl;
    float pen = WHEEL_R - (cy - gy) * ny;
    w->contact[i] = pen > 0;
    if (pen <= 0) { w->spin[i] += w->spin[i] > 0 ? -h : 0; continue; }
    air = false;
    float vcx = w->vx - w->w * oy, vcy = w->vy + w->w * ox;
    float vn = vcx * nx + vcy * ny;
    float fn = SPRING * pen - DAMP * vn;
    if (fn < 0) fn = 0;
    float tx = ny, ty = -nx;
    float vt = vcx * tx + vcy * ty;
    float ft = -vt * 0.25f;
    if (control) {
      if (i == 0 && throttle) ft += DRIVE * (w->turboT > 0 ? 1.7f : 1.0f);
      if (brake) ft += vt > 0.4f ? -BRAKE : (i == 0 ? -REVERSE : 0);
    }
    float lim = MU * fn;
    if (ft > lim) ft = lim;
    if (ft < -lim) ft = -lim;
    float Fx = fn * nx + ft * tx, Fy = fn * ny + ft * ty;
    fx += Fx; fy += Fy;
    tq += ox * Fy - oy * Fx;
    w->spin[i] += vt / WHEEL_R * h;
  }
  if (control) tq += lean * (air ? 12.0f : 3.5f) * INERTIA;
  tq -= w->w * (air ? 0.25f : 0.8f);
  float sp = sqrtf(w->vx * w->vx + w->vy * w->vy);
  fx -= w->vx * sp * DRAG;
  fy -= w->vy * sp * DRAG;
  w->vx += fx / MASS * h;
  w->vy += fy / MASS * h;
  w->w += tq / INERTIA * h;
  w->x += w->vx * h;
  w->y += w->vy * h;
  w->a += w->w * h;
  if (air) w->airRot += w->w * h;
}

static void checkCrash() {
  float hx, hy;
  rot(w->a, -0.1f, 0.95f, hx, hy);
  if (w->y + hy < groundAt(w->x + hx) + 0.05f || w->y < groundAt(w->x) + 0.05f) crash();
}

static void finishRun() {
  w->mode = M_FINISH;
  w->modeT = 0;
  float t = w->time - w->bonus;
  if (t < 0) t = 0;
  w->medal = t <= w->gold ? 3 : t <= w->silver ? 2 : t <= w->bronze ? 1 : 0;
  hwLog("Motokros: %s bitti %.2f sn, takla %d, madalya %d", w->name, (double)t, w->flips, w->medal);
  char key[16];
  save::keyFor(key, "mxt", w->name);
  w->newBest = save::setMin(key, (int32_t)(t * 1000));
  save::keyFor(key, "mxm", w->name);
  int prev = save::get(key, 0);
  if (w->medal > prev) {
    save::set(key, w->medal);
    static const char* const AGG[4] = { "", "mx_b", "mx_s", "mx_g" };
    if (prev) save::set(AGG[prev], save::get(AGG[prev], 0) - 1);
    save::set(AGG[w->medal], save::get(AGG[w->medal], 0) + 1);
  }
  w->unlockedNew = false;
  if (w->medal > 0 && w->trackIdx + 2 > w->unlocked && w->trackIdx + 1 < w->tracks.count) {
    w->unlocked = w->trackIdx + 2;
    save::set("mx_unl", w->unlocked);
    w->unlockedNew = true;
  }
  input::clear();
}

static void update(float dt) {
  w->modeT += dt;
  if (w->msgT > 0) w->msgT -= dt;
  switch (w->mode) {
    case M_SELECT: {
      int r = w->list.update(w->tracks.count, dt, 6);
      if (r == -2) app::requestMenu();
      else if (r >= 0) {
        if (r < w->unlocked) startRun(r);
        else { snprintf(w->msg, sizeof(w->msg), "BRONZ AL, AC"); w->msgT = 1.5f; }
      }
      return;
    }
    case M_COUNT:
      if (w->modeT >= 2.0f) { w->mode = M_RIDE; w->modeT = 0; }
      break;
    case M_PAUSE:
      if (input::pressed(B_RELOAD) || input::pressed(B_FIRE)) { w->mode = M_RIDE; input::clear(); }
      return;
    case M_FINISH:
      if (w->modeT > 1.0f && input::pressed(B_FIRE)) { w->mode = M_SELECT; input::clear(); return; }
      break;
    default:
      break;
  }
  bool ride = w->mode == M_RIDE;
  if (ride) {
    w->time += dt;
    if (input::pressed(B_RELOAD)) { respawn(); return; }   // kontrol noktasına dön
    if (input::pressed(B_FIRE) && w->turbos > 0 && w->turboT <= 0) { w->turbos--; w->turboT = 1.5f; }
  }
  if (w->turboT > 0) w->turboT -= dt;
  bool thr = ride && input::down(B_FWD);
  bool brk = ride && input::down(B_BACK);
  float lean = 0;
  if (ride && input::down(B_LEFT)) lean += 1;    // arkaya yaslan (burun yukarı)
  if (ride && input::down(B_RIGHT)) lean -= 1;   // öne yaslan
  bool control = ride || w->mode == M_FINISH;
  if (w->mode == M_COUNT) { w->vx = w->vy = w->w = 0; }
  const int SUB = 4;
  for (int s = 0; s < SUB; s++) physicsStep(dt / SUB, thr, brk, lean, control);

  bool air = !w->contact[0] && !w->contact[1];
  if (air) w->airT += dt;
  if (ride && !air && w->wasAir) {
    int n = (int)(fabsf(w->airRot) / (2 * PI_F) + 0.15f);   // tam dönüş sayısı
    if (n > 0 && w->airT > 0.4f) {
      w->flips += n;
      w->bonus += n * 1.0f;
      if (n > 1) snprintf(w->msg, sizeof(w->msg), "%dx TAKLA! -%ds", n % 10, n % 10);
      else snprintf(w->msg, sizeof(w->msg), "TAKLA! -1s");
      w->msgT = 1.3f;
    }
  }
  if (!air) { w->airRot = 0; w->airT = 0; }
  w->wasAir = air;

  if (ride) {
    checkCrash();
    for (int i = 0; i < w->nObj; i++) {
      Obj& o = w->objs[i];
      if (o.type == O_CHECKPOINT && !o.passed && w->x >= o.x) {
        o.passed = true;
        w->cpX = o.x;
        w->cpIdx++;
        snprintf(w->msg, sizeof(w->msg), "KONTROL NOKTASI");
        w->msgT = 1.0f;
      }
    }
    if (w->x >= w->finishX) finishRun();
    // çukurda ya da duvar önünde sıkıştıysa kendiliğinden kontrol noktasına dön
    float sp = sqrtf(w->vx * w->vx + w->vy * w->vy);
    w->stuckT = (thr && sp < 1.5f) ? w->stuckT + dt : 0;
    if (w->stuckT > 2.5f) {
      snprintf(w->msg, sizeof(w->msg), "SIKISTIN");
      w->msgT = 1.2f;
      respawn();
    }
  } else if (w->mode == M_CRASH && w->modeT > 1.5f) {
    respawn();
    w->mode = M_RIDE;
    w->modeT = 0;
  }
  // kamera
  float tx = w->x + w->vx * 0.3f, ty = w->y;
  float k = dt * 5 > 1 ? 1 : dt * 5;
  w->camX += (tx - w->camX) * k;
  w->camY += (ty - w->camY) * k * 0.8f;
}

// ---------- çizim ----------
struct Pal { uint16_t skyTop, skyBot, far, mid, top, dirt1, dirt2; };
static Pal palette() {
  switch (w->theme) {
    case TH_DESERT: return { rgb(240, 150, 70), rgb(255, 220, 150), rgb(200, 120, 90), rgb(220, 150, 90),
                             rgb(240, 200, 120), rgb(205, 150, 80), rgb(185, 130, 65) };
    case TH_NIGHT:  return { rgb(5, 5, 25), rgb(40, 30, 80), rgb(30, 25, 60), rgb(20, 30, 45),
                             rgb(70, 110, 70), rgb(70, 50, 40), rgb(55, 40, 32) };
    case TH_SNOW:   return { rgb(120, 160, 210), rgb(220, 230, 245), rgb(170, 180, 205), rgb(200, 210, 225),
                             rgb(250, 250, 255), rgb(150, 140, 135), rgb(130, 120, 115) };
    default:        return { rgb(60, 140, 230), rgb(180, 220, 250), rgb(90, 140, 170), rgb(60, 130, 80),
                             rgb(80, 180, 60), rgb(120, 85, 50), rgb(105, 72, 42) };
  }
}

static inline int sx(float x) { return ORG_X + (int)lroundf((x - w->camX) * PPM); }
static inline int sy(float y) { return ORG_Y - (int)lroundf((y - w->camY) * PPM); }

static void drawObj(const Obj& o, const Pal& p) {
  int x = sx(o.x);
  if (x < -30 || x > SCR_W + 30) return;
  int y = sy(groundAt(o.x));
  switch (o.type) {
    case O_CHECKPOINT: {
      uint16_t c = o.passed ? rgb(60, 220, 80) : rgb(240, 60, 40);
      gfx::vline(x, y - 22, 22, rgb(220, 220, 220));
      gfx::fillTriangle(x + 1, y - 22, x + 12, y - 18, x + 1, y - 14, c);
      break;
    }
    case O_FLAG:
      gfx::vline(x, y - 16, 16, rgb(180, 180, 180));
      gfx::fillRect(x + 1, y - 16, 8, 5, rgb(250, 210, 30));
      break;
    case O_TREE:
      if (w->theme == TH_SNOW || w->theme == TH_NIGHT) {
        uint16_t g = w->theme == TH_NIGHT ? rgb(20, 50, 35) : rgb(30, 90, 55);
        gfx::fillTriangle(x, y - 30, x - 9, y - 10, x + 9, y - 10, g);
        gfx::fillTriangle(x, y - 38, x - 7, y - 22, x + 7, y - 22, g);
        gfx::fillRect(x - 1, y - 10, 3, 10, rgb(90, 60, 35));
      } else {
        gfx::fillRect(x - 1, y - 14, 3, 14, rgb(100, 65, 35));
        gfx::fillCircle(x, y - 20, 8, rgb(40, 130, 50));
        gfx::fillCircle(x - 3, y - 22, 4, rgb(70, 170, 70));
      }
      break;
    case O_CACTUS:
      gfx::fillRect(x - 2, y - 18, 4, 18, rgb(50, 150, 70));
      gfx::fillRect(x - 7, y - 13, 3, 6, rgb(50, 150, 70));
      gfx::fillRect(x - 7, y - 9, 6, 2, rgb(50, 150, 70));
      gfx::fillRect(x + 4, y - 15, 3, 6, rgb(50, 150, 70));
      gfx::fillRect(x + 1, y - 11, 6, 2, rgb(50, 150, 70));
      break;
    case O_ROCK:
      gfx::fillEllipse(x, y - 2, 6, 4, rgb(130, 125, 120));
      gfx::fillEllipse(x - 2, y - 3, 3, 2, rgb(170, 165, 160));
      break;
    case O_SIGN:
      gfx::vline(x, y - 12, 12, rgb(120, 120, 120));
      gfx::fillRect(x - 6, y - 18, 12, 7, rgb(250, 210, 30));
      gfx::fillTriangle(x + 4, y - 15, x - 1, y - 17, x - 1, y - 13, rgb(30, 30, 30));
      break;
    case O_TIRE:
      gfx::fillCircle(x, y - 3, 3, rgb(30, 30, 30));
      gfx::pixel(x, y - 3, rgb(90, 90, 90));
      break;
    default: break;
  }
  (void)p;
}

static void drawTerrain(const Pal& p) {
  for (int x = 0; x < SCR_W; x++) {
    float wx = w->camX + (x - ORG_X) / PPM;
    int top = sy(groundAt(wx));
    if (top < 0) top = 0;
    if (top >= SCR_H) continue;
    int wpx = (int)floorf(wx * PPM);
    gfx::vline(x, top, 2, p.top);
    for (int y = top + 2; y < SCR_H; y++) {
      int d = y - top;
      uint32_t h = hash32((uint32_t)wpx * 31u + (uint32_t)(d / 3) * 977u);
      uint16_t c = ((d / 6 + (wpx >> 3)) & 1) ? p.dirt1 : p.dirt2;
      if ((h & 15) == 0) c = shade(c, 170);
      if (d > 40) c = shade(c, 256 - (d - 40) * 3 > 120 ? 256 - (d - 40) * 3 : 120);
      fb[y * SCR_W + x] = sw16(c);
    }
  }
  // bitiş çizgisi
  int fx = sx(w->finishX);
  if (fx > -10 && fx < SCR_W + 10) {
    int fy = sy(groundAt(w->finishX));
    for (int i = 0; i < 8; i++)
      gfx::fillRect(fx - 3 + (i & 1) * 3, fy - 40 + i * 3, 3, 3, C_WHITE);
    for (int i = 0; i < 8; i++)
      gfx::fillRect(fx - 3 + ((i + 1) & 1) * 3, fy - 40 + i * 3, 3, 3, C_BLACK);
    gfx::vline(fx - 4, fy - 40, 40, rgb(200, 200, 200));
  }
}

static void bikePoint(float bx, float by, float& ox, float& oy) {
  float rx, ry;
  rot(w->a, bx, by, rx, ry);
  ox = (float)sx(w->x + rx);
  oy = (float)sy(w->y + ry);
}

static void drawBike(bool crashed) {
  float rx, ry, fx, fy;
  bikePoint(WOFF[0][0], WOFF[0][1], rx, ry);
  bikePoint(WOFF[1][0], WOFF[1][1], fx, fy);
  int r = (int)(WHEEL_R * PPM);
  for (int i = 0; i < 2; i++) {
    int cx = (int)(i ? fx : rx), cy = (int)(i ? fy : ry);
    gfx::fillCircle(cx, cy, r, rgb(25, 25, 25));
    gfx::circle(cx, cy, r, rgb(70, 70, 70));
    for (int k = 0; k < 2; k++) {
      float sa = w->spin[i] * -1.0f + k * PI_F / 2;
      gfx::line(cx - (int)(cosf(sa) * (r - 1)), cy - (int)(sinf(sa) * (r - 1)),
                cx + (int)(cosf(sa) * (r - 1)), cy + (int)(sinf(sa) * (r - 1)), rgb(160, 160, 160));
    }
  }
  float sx0, sy0, sx1, sy1, hx, hy, tx, ty, px, py;
  bikePoint(-0.25f, 0.02f, sx0, sy0);      // sele arkası
  bikePoint(0.30f, 0.12f, sx1, sy1);       // depo önü
  bikePoint(0.45f, 0.35f, hx, hy);         // gidon
  uint16_t body = rgb(230, 60, 20);
  gfx::thickLine((int)rx, (int)ry, (int)sx0, (int)sy0, 2, rgb(80, 80, 90));     // arka salıncak
  gfx::thickLine((int)fx, (int)fy, (int)hx, (int)hy, 2, rgb(180, 180, 190));    // ön çatal
  gfx::thickLine((int)sx0, (int)sy0, (int)sx1, (int)sy1, 3, body);              // gövde
  gfx::thickLine((int)sx1, (int)sy1, (int)hx, (int)hy, 2, body);
  if (w->turboT > 0) {
    float ex, ey;
    bikePoint(-0.7f, 0.0f, ex, ey);
    gfx::fillCircle((int)ex, (int)ey, 2 + (int)(hash32((uint32_t)(w->time * 30)) & 1), rgb(80, 180, 255));
  }
  if (crashed) return;   // sürücü düştü
  // sürücü: kalça sele üstünde, gövde eğilmeye göre
  float lean = (input::down(B_RIGHT) ? 0.25f : 0) - (input::down(B_LEFT) ? 0.25f : 0);
  bikePoint(-0.1f, 0.25f, px, py);                   // kalça
  bikePoint(0.05f + lean, 0.8f, tx, ty);             // omuz
  float kx, ky, fx2, fy2, hdx, hdy;
  bikePoint(0.2f, 0.12f, kx, ky);                    // diz
  bikePoint(0.0f, -0.12f, fx2, fy2);                 // ayak
  bikePoint(0.1f + lean * 1.2f, 1.02f, hdx, hdy);    // kask
  gfx::thickLine((int)px, (int)py, (int)kx, (int)ky, 2, rgb(40, 40, 120));
  gfx::thickLine((int)kx, (int)ky, (int)fx2, (int)fy2, 2, rgb(40, 40, 120));
  gfx::thickLine((int)px, (int)py, (int)tx, (int)ty, 3, rgb(240, 240, 240));
  gfx::thickLine((int)tx, (int)ty, (int)hx, (int)hy, 2, rgb(240, 240, 240));
  gfx::fillCircle((int)hdx, (int)hdy, 3, rgb(240, 200, 30));
  float vx2, vy2;
  bikePoint(0.25f + lean * 1.2f, 1.02f, vx2, vy2);
  gfx::pixel((int)vx2, (int)vy2, rgb(30, 30, 60));
}

static void drawBackground(const Pal& p) {
  gfx::vgradient(0, 0, SCR_W, SCR_H, p.skyTop, p.skyBot);
  if (w->theme == TH_NIGHT) {
    for (int i = 0; i < 26; i++) {
      int x = ((int)(hash32(i * 11) % 512 - w->camX * 0.5f) % SCR_W + SCR_W) % SCR_W;
      gfx::pixel(x, (int)(hash32(i * 5 + 1) % 60), C_WHITE);
    }
    gfx::fillCircle(100, 18, 7, rgb(240, 240, 220));
  } else {
    gfx::fillCircle(100, 20, 8, w->theme == TH_DESERT ? rgb(255, 250, 200) : rgb(255, 245, 180));
  }
  for (int x = 0; x < SCR_W; x++) {
    float a = (x + w->camX * PPM * 0.15f) * 0.03f;
    int h1 = (int)(26 + sinf(a) * 10 + sinf(a * 2.3f + 1) * 5);
    float b = (x + w->camX * PPM * 0.4f) * 0.05f;
    int h2 = (int)(14 + sinf(b) * 6 + sinf(b * 3.1f) * 3);
    int base = ORG_Y + 20 - (int)((w->camY) * PPM * 0.2f);
    gfx::vline(x, base - h1, SCR_H, p.far);
    gfx::vline(x, base + 10 - h2, SCR_H, p.mid);
  }
}

static void renderRide() {
  Pal p = palette();
  drawBackground(p);
  for (int i = 0; i < w->nObj; i++) drawObj(w->objs[i], p);
  drawTerrain(p);
  drawBike(w->mode == M_CRASH);
  if (w->mode == M_CRASH && w->modeT < 0.3f) gfx::blend(0, 0, SCR_W, SCR_H, C_RED, 80);

  // HUD
  gfx::darken(0, 0, SCR_W, 11, 120);
  char tb[16];
  float t = w->time - w->bonus;
  ui::formatTime(tb, sizeof(tb), (uint32_t)((t < 0 ? 0 : t) * 1000));
  gfx::text(2, 2, tb, C_WHITE);
  float prog = w->x / w->finishX;
  ui::bar(50, 4, 44, 3, prog, rgb(250, 210, 30), rgb(40, 40, 40));
  for (int i = 0; i < w->nObj; i++)
    if (w->objs[i].type == O_CHECKPOINT) {
      int cx = 50 + (int)(44 * w->objs[i].x / w->finishX);
      gfx::vline(cx, 2, 7, w->objs[i].passed ? rgb(60, 220, 80) : rgb(240, 60, 40));
    }
  for (int i = 0; i < 3; i++) gfx::fillRect(100 + i * 9, 2, 7, 7, i < w->turbos ? rgb(60, 180, 255) : rgb(40, 50, 60));
  int kmh = (int)(sqrtf(w->vx * w->vx + w->vy * w->vy) * 3.6f);
  gfx::textf(SCR_W - 2, 119, C_WHITE, 1, AL_RIGHT, "%d km/sa", kmh);
  if (w->flips) gfx::textf(2, 119, rgb(255, 210, 60), 1, AL_LEFT, "Takla %d", w->flips);
  if (w->msgT > 0) gfx::textShadow(SCR_W / 2, 18, w->msg, rgb(255, 230, 80), 1, AL_CENTER);
}

static const uint16_t MEDAL_COL[4] = { rgb(90, 90, 90), rgb(200, 120, 60), rgb(200, 200, 215), rgb(255, 210, 40) };
static const char* const MEDAL_NAME[4] = { "MADALYA YOK", "BRONZ", "GUMUS", "ALTIN" };

static void selectRow(int i, int x, int y, int wd, int h, bool sel, float t) {
  (void)t;
  bool locked = i >= w->unlocked;
  if (sel) gfx::fillRect(x, y, wd - 4, h - 1, rgb(80, 50, 10));
  char b[32];
  snprintf(b, sizeof(b), "%d %.13s", i + 1, w->tracks.title[i]);
  gfx::text(x + 3, y + 2, b, locked ? rgb(100, 90, 70) : (sel ? C_WHITE : rgb(230, 210, 170)));
  char key[16];
  save::keyFor(key, "mxm", w->tracks.title[i]);
  int m = save::get(key, 0);
  if (locked) {
    gfx::fillRect(x + wd - 13, y + 5, 7, 5, rgb(120, 110, 60));
    gfx::rect(x + wd - 12, y + 2, 5, 4, rgb(120, 110, 60));
  } else if (m) {
    gfx::fillCircle(x + wd - 10, y + 5, 3, MEDAL_COL[m]);
  }
}

static void renderSelect() {
  gfx::vgradient(0, 0, SCR_W, SCR_H, rgb(70, 40, 10), rgb(10, 5, 0));
  for (int x = 0; x < SCR_W; x++) {
    int h = (int)(8 + sinf((x + w->modeT * 20) * 0.08f) * 4 + sinf(x * 0.21f) * 2);
    gfx::vline(x, SCR_H - h, h, rgb(110, 70, 30));
  }
  gfx::textShadow(SCR_W / 2, 4, "MOTOKROS", rgb(255, 170, 40), 2, AL_CENTER);
  gfx::text(SCR_W / 2, 22, "PARKUR SEC", C_GREY, 1, AL_CENTER);
  ui::listDraw(w->list, w->tracks.count, 2, 34, SCR_W - 4, 12, 6, selectRow, w->modeT);
  gfx::fillRect(0, 108, SCR_W, 20, C_BLACK);
  if (w->list.sel < w->tracks.count) {
    char key[16], tb[16];
    save::keyFor(key, "mxt", w->tracks.title[w->list.sel]);
    int32_t bt = save::get(key, -1);
    if (bt >= 0) { ui::formatTime(tb, sizeof(tb), (uint32_t)bt); gfx::textf(SCR_W / 2, 110, rgb(255, 200, 120), 1, AL_CENTER, "En iyi %s", tb); }
    else gfx::text(SCR_W / 2, 110, "Bronz: yeni parkur", rgb(200, 170, 130), 1, AL_CENTER);
  }
  gfx::textf(SCR_W / 2, 120, rgb(130, 110, 90), 1, AL_CENTER, sd::ok() ? "SD: %d ek parkur" : "SD yok: %d parkur",
             sd::ok() ? w->tracks.count - w->tracks.builtinCount : w->tracks.count);
  if (w->msgT > 0) gfx::textShadow(SCR_W / 2, 96, w->msg, rgb(255, 120, 80), 1, AL_CENTER);
}

static void render() {
  if (w->mode == M_SELECT) { renderSelect(); return; }
  renderRide();
  if (w->mode == M_COUNT) {
    gfx::textShadow(SCR_W / 2, 30, w->modeT < 1.0f ? w->name : "HAZIR", C_WHITE, 1, AL_CENTER);
  } else if (w->mode == M_PAUSE) {
    ui::banner("DURDU", "RELOAD: devam", C_WHITE);
  } else if (w->mode == M_FINISH) {
    ui::panel(10, 12, SCR_W - 20, 104, rgb(220, 140, 40), rgb(25, 12, 0));
    gfx::textShadow(SCR_W / 2, 18, "BITIS!", C_WHITE, 1, AL_CENTER);
    int m = w->medal;
    gfx::fillCircle(SCR_W / 2, 37, 9, MEDAL_COL[m]);
    gfx::fillCircle(SCR_W / 2, 37, 6, shade(MEDAL_COL[m], 200));
    gfx::text(SCR_W / 2, 50, MEDAL_NAME[m], MEDAL_COL[m], 1, AL_CENTER);
    char tb[16];
    float t = w->time - w->bonus;
    ui::formatTime(tb, sizeof(tb), (uint32_t)(t * 1000));
    gfx::text(16, 62, "Sure", C_GREY);
    gfx::text(SCR_W - 16, 62, tb, w->newBest ? rgb(255, 210, 40) : C_WHITE, 1, AL_RIGHT);
    gfx::text(16, 72, "Takla", C_GREY);
    gfx::textf(SCR_W - 16, 72, C_WHITE, 1, AL_RIGHT, "%d", w->flips);
    ui::formatTime(tb, sizeof(tb), (uint32_t)(w->gold * 1000));
    gfx::text(16, 82, "Altin", rgb(255, 210, 40));
    gfx::text(SCR_W - 16, 82, tb, rgb(255, 210, 40), 1, AL_RIGHT);
    if (w->unlockedNew) gfx::text(SCR_W / 2, 94, "YENI PARKUR ACILDI!", rgb(80, 255, 120), 1, AL_CENTER);
    if (w->modeT > 1.0f) gfx::text(SCR_W / 2, 105, "ATES: devam", C_GREY, 1, AL_CENTER);
  }
}

// ---------- Game arayüzü ----------
static bool enter() {
  void* m = calloc(1, sizeof(World));
  if (!m) return false;
  w = new (m) World;
  w->tracks.build(BUILTIN_TRACKS, BUILTIN_COUNT, "/moto", ".mtb");
  w->unlocked = save::get("mx_unl", 1);
  if (w->unlocked > w->tracks.count) w->unlocked = w->tracks.count;
  if (w->unlocked < 1) w->unlocked = 1;
  w->list.sel = w->unlocked - 1;
  w->mode = M_SELECT;
  hwLog("Motokros: World %u B, %d parkur", (unsigned)sizeof(World), w->tracks.count);
  return true;
}

static void exitGame() {
  if (w) { w->~World(); free(w); }
  w = nullptr;
}

static void icon(int x, int y, float t) {
  gfx::vgradient(x, y, 24, 24, rgb(240, 150, 70), rgb(255, 220, 150));
  gfx::fillTriangle(x, y + 24, x + 16, y + 14, x + 16, y + 24, rgb(185, 130, 65));
  gfx::fillRect(x + 16, y + 20, 8, 4, rgb(185, 130, 65));
  int jump = t > 0 ? (int)(fabsf(sinf(t * 3)) * 3) : 0;
  int bx = x + 12, by = y + 10 - jump;
  gfx::fillCircle(bx - 5, by + 4, 3, rgb(25, 25, 25));
  gfx::fillCircle(bx + 5, by + 2, 3, rgb(25, 25, 25));
  gfx::thickLine(bx - 4, by + 2, bx + 3, by, 2, rgb(230, 60, 20));
  gfx::thickLine(bx - 1, by, bx + 1, by - 6, 2, rgb(240, 240, 240));
  gfx::fillCircle(bx + 2, by - 7, 2, rgb(240, 200, 30));
}

static void scoreText(char* buf, int n) {
  int total = BUILTIN_COUNT + sd::count("/moto", ".mtb");
  int u = save::get("mx_unl", 1);
  if (u > total) u = total;
  snprintf(buf, n, "%d/%d A%d G%d B%d", u, total, (int)save::get("mx_g", 0), (int)save::get("mx_s", 0),
           (int)save::get("mx_b", 0));
}

static const char* const CONTROLS[] = {
  "ILERI", "Gaz",
  "GERI", "Fren/geri",
  "SOLA", "Arkaya yaslan",
  "SAGA", "One yaslan",
  "ATES", "Turbo (3)",
  "RELOAD", "Kontrol nok.",
  nullptr, nullptr,
};

#ifdef HOST
// Simülatördeki test sürücüsü için durum (yalnızca bilgisayar derlemesinde)
bool debugState(float* x, float* a, float* slopeAhead, float* slopeHere, bool* air) {
  if (!w || w->mode != M_RIDE) return false;
  float s1, s2;
  groundAt(w->x + 2.5f, &s1);
  groundAt(w->x, &s2);
  *x = w->x; *a = w->a; *slopeAhead = atanf(s1); *slopeHere = atanf(s2);
  *air = !w->contact[0] && !w->contact[1];
  return true;
}
#endif

}  // namespace moto

extern const Game MOTO_GAME = {
  "MOTOKROS", moto::icon, moto::scoreText, moto::CONTROLS,
  moto::enter, moto::update, moto::render, moto::exitGame,
};
