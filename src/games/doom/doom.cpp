// DOOM — karanlık raycaster: 3 düşman türü + boss, 3 silah (şarjör/yedek, reload),
// kapılar, renkli anahtarlar, gizli odalar, 2 gömülü + SD karttan ek bölümler.
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
#include "levels_builtin.h"
#include "sprites.h"
#include "textures.h"

namespace doom {

static const int HUD_H  = 18;
static const int VIEW_H = SCR_H - HUD_H;
static const int MAXM   = 32;
static const int MAX_EN = 48, MAX_IT = 80, MAX_PR = 16, MAX_DR = 32, MAX_PUFF = 10, MAX_DROP = 10;
static const int MAX_VIS = MAX_EN + MAX_IT + MAX_PR + MAX_PUFF;
static const float PI_F = 3.14159265f;

// ---------- düşman türleri ----------
enum EType : uint8_t { E_ZOMBIE, E_DEMON, E_CASTER, E_BOSS };
struct ETypeInfo {
  int hp; float speed, radius, height, lift;
  int dmin, dmax; float range, windup, cooldown;
  bool ranged; float pain;
  ArtId walk, atk, dead;
};
static const ETypeInfo ETYPES[] = {
  //  hp  hiz   r     boy   havada  hasar   menzil hazirlik bekleme uzak  aci
  {   90, 0.85f, 0.3f, 0.92f, 0.0f,  8, 14, 0.95f, 0.60f, 1.5f, false, 0.55f, A_ZOMBIE, A_ZOMBIE_ATK, A_ZOMBIE_DEAD },
  {   45, 2.9f,  0.3f, 0.75f, 0.0f,  6, 11, 0.90f, 0.25f, 0.9f, false, 0.35f, A_DEMON,  A_DEMON_ATK,  A_DEMON_DEAD  },
  {   55, 1.3f,  0.3f, 0.85f, 0.15f, 12, 20, 9.0f, 0.50f, 2.2f, true,  0.50f, A_CASTER, A_CASTER_ATK, A_CASTER_DEAD },
  { 1000, 1.5f,  0.55f, 1.55f, 0.0f, 25, 40, 1.4f, 0.55f, 1.5f, true,  0.06f, A_BOSS,   A_BOSS_ATK,   A_BOSS_DEAD   },
};
enum EState : uint8_t { ES_IDLE, ES_CHASE, ES_WINDUP, ES_PAIN, ES_DYING, ES_DEAD };

struct Enemy {
  float x, y, t, cd, anim, flash, seeT, strafeT;
  int16_t hp, scrX, scrW;
  float depth;
  uint8_t type, st;
  bool sees;
  int8_t strafe;
};

enum IKind : uint8_t { I_HEALTH, I_ARMOR, I_BULLETS, I_SHELLS, I_SHOTGUN, I_MG, I_KEY_R, I_KEY_Y, I_KEY_B, I_TORCH, I_CORPSE };
struct Item { float x, y; uint8_t kind; bool gone; };
struct Proj { float x, y, dx, dy, life; int16_t dmg; bool alive, boss; };
struct Door { uint8_t x, y, key, tex; bool secret; int8_t dir; float open, wait; };
struct Puff { float x, y, t; bool blood, alive; };
struct Drop { float x, y, life; uint8_t r; };

enum Weapon : uint8_t { W_PISTOL, W_SHOTGUN, W_MG, W_COUNT };
struct WInfo { const char* name; int dmin, dmax, pellets; float rate, spread, reload; int mag, maxRes; };
static const WInfo WEAPONS[W_COUNT] = {
  { "TABANCA",  12, 18, 1, 0.36f, 0.012f, 1.0f, 12, 150 },
  { "POMPALI",   7, 11, 7, 0.85f, 0.085f, 1.7f,  6,  50 },
  { "MAKINELI",  9, 13, 1, 0.09f, 0.035f, 1.9f, 40, 300 },
};

struct Inventory { int hp, armor; uint8_t owned, weapon; int mag[W_COUNT], res[W_COUNT]; };

enum Mode : uint8_t { MD_SELECT, MD_PLAY, MD_DEAD, MD_STATS, MD_VICTORY };
enum VisKind : uint8_t { V_ENEMY, V_ITEM, V_PROJ, V_PUFF };
struct Vis { float depth, tx; uint8_t kind; uint8_t idx; };

struct World {
  LevelSet levels;
  ui::List list;
  int levelIdx, unlocked;
  Mode mode;
  float modeT;

  // harita
  int mw, mh;
  uint8_t grid[MAXM * MAXM];     // 0 boş, 1..6 duvar, 7 çıkış, 0x80|i kapı
  uint8_t flow[MAXM * MAXM];     // oyuncuya BFS uzaklığı (düşman yol bulma)
  uint16_t queue[MAXM * MAXM];
  float flowT;
  char name[20];
  int par, light, fog;
  uint16_t fogColor, ceilColor, floorColor;
  bool flicker;
  Door doors[MAX_DR]; int nDoors;
  Enemy en[MAX_EN]; int nEn;
  Item it[MAX_IT]; int nIt;
  Proj pr[MAX_PR];
  Puff puffs[MAX_PUFF];
  Drop drops[MAX_DROP];
  float torchX[12], torchY[12];
  int nTorch;

  // oyuncu
  float px, py, pa, dirX, dirY, plX, plY, camH;
  Inventory inv, startInv;
  uint8_t keys;
  float fireCd, reloadT, switchT, flashT, hurtT, lightBoost, bobT, lastReloadPress, msgT, levelTime, noiseT, emptyMsgT;
  bool walking, reloading;
  int kills, totalKills, secrets, totalSecrets;
  char msg[28];
  uint16_t msgColor;
  float flick, flickT, flickTarget;

  // çizim
  uint16_t tex[T_COUNT * TEX * TEX];
  uint16_t pal[128];
  float zbuf[SCR_W];
  uint16_t rowColor[VIEW_H];
  Vis vis[MAX_VIS];
  int horizon;
};
static World* w = nullptr;

static inline float frand() { return (hwRandom() & 0xFFFFFF) / 16777216.0f; }
static inline int irand(int a, int b) { return a + (int)(hwRandom() % (uint32_t)(b - a + 1)); }

// ---------- palet ----------
static bool fullbright(char c) { return c == 'O' || c == 'Y' || c == 'E' || c == 'e' || c == 'L' || c == 'F' || c == 'C'; }

static uint16_t palColor(char c) {
  switch (c) {
    case 'K': return rgb(20, 10, 10);    case 'k': return rgb(50, 45, 45);
    case 'W': return rgb(220, 210, 190); case 'w': return rgb(150, 140, 125);
    case 'R': return rgb(200, 10, 10);   case 'r': return rgb(110, 0, 0);
    case 'M': return rgb(190, 80, 80);   case 'm': return rgb(120, 40, 40);
    case 'S': return rgb(170, 160, 130); case 's': return rgb(110, 100, 80);
    case 'G': return rgb(90, 120, 60);   case 'g': return rgb(50, 70, 35);
    case 'B': return rgb(110, 70, 40);   case 'b': return rgb(60, 35, 20);
    case 'P': return rgb(90, 40, 110);   case 'p': return rgb(50, 20, 60);
    case 'O': return rgb(255, 140, 0);   case 'Y': return rgb(255, 230, 60);
    case 'E': return rgb(255, 30, 0);    case 'e': return rgb(100, 255, 80);
    case 'C': return rgb(80, 220, 255);  case 'H': return rgb(200, 190, 160);
    case 'A': return rgb(130, 130, 140); case 'a': return rgb(70, 70, 80);
    case 'L': return rgb(255, 255, 240); case 'F': return rgb(255, 80, 0);
    case 'T': return rgb(240, 240, 220); case 'Z': return rgb(40, 80, 220);
    case 'z': return rgb(20, 30, 110);   case 'V': return rgb(40, 200, 60);
    case 'v': return rgb(20, 110, 30);
    default:  return rgb(255, 0, 255);
  }
}

// ---------- harita ----------
static inline uint8_t g(int x, int y) {
  if (x < 0 || y < 0 || x >= w->mw || y >= w->mh) return 1;
  return w->grid[y * MAXM + x];
}
static inline bool solid(int x, int y) {
  uint8_t c = g(x, y);
  if (!c) return false;
  if (c & 0x80) return w->doors[c & 0x7F].open < 0.85f;
  return true;
}
static bool freeBox(float x, float y, float r) {
  return !solid((int)(x - r), (int)(y - r)) && !solid((int)(x + r), (int)(y - r)) &&
         !solid((int)(x - r), (int)(y + r)) && !solid((int)(x + r), (int)(y + r));
}
static void tryMove(float& x, float& y, float mx, float my, float r) {
  if (freeBox(x + mx, y, r)) x += mx;
  if (freeBox(x, y + my, r)) y += my;
}
static bool lineOfSight(float x0, float y0, float x1, float y1) {
  float dx = x1 - x0, dy = y1 - y0;
  float d = sqrtf(dx * dx + dy * dy);
  int steps = (int)(d / 0.2f) + 1;
  for (int i = 1; i < steps; i++) {
    float t = (float)i / steps;
    if (solid((int)(x0 + dx * t), (int)(y0 + dy * t))) return false;
  }
  return true;
}

static void message(const char* m, uint16_t c) {
  snprintf(w->msg, sizeof(w->msg), "%s", m);
  w->msgColor = c;
  w->msgT = 2.0f;
}

// ---------- bölüm yükleme ----------
static void addEnemy(EType t, float x, float y) {
  if (w->nEn >= MAX_EN) return;
  Enemy& e = w->en[w->nEn++];
  memset(&e, 0, sizeof(e));
  e.x = x; e.y = y; e.type = t; e.hp = (int16_t)ETYPES[t].hp;
  e.st = ES_IDLE; e.cd = 1.0f + frand(); e.seeT = frand() * 0.3f;
  e.strafe = (hwRandom() & 1) ? 1 : -1;
  w->totalKills++;
}
static void addItem(IKind k, float x, float y) {
  if (w->nIt >= MAX_IT) return;
  Item& i = w->it[w->nIt++];
  i.x = x; i.y = y; i.kind = k; i.gone = false;
}

static void parseCell(int x, int y, char c, float& sx, float& sy) {
  uint8_t& cell = w->grid[y * MAXM + x];
  float cx = x + 0.5f, cy = y + 0.5f;
  cell = 0;
  switch (c) {
    case '1': case '2': case '3': case '4': case '5': case '6': cell = (uint8_t)(c - '0'); break;
    case 'X': cell = 7; break;
    case 'D': case 'R': case 'Y': case 'B': case 'S': {
      if (w->nDoors >= MAX_DR) { cell = 1; break; }
      Door& d = w->doors[w->nDoors];
      memset(&d, 0, sizeof(d));
      d.x = (uint8_t)x; d.y = (uint8_t)y;
      d.key = c == 'R' ? 1 : c == 'Y' ? 2 : c == 'B' ? 3 : 0;
      d.secret = c == 'S';
      d.tex = c == 'R' ? T_DOOR_R : c == 'Y' ? T_DOOR_Y : c == 'B' ? T_DOOR_B : T_DOOR;
      if (d.secret) w->totalSecrets++;
      cell = (uint8_t)(0x80 | w->nDoors++);
      break;
    }
    case '@': sx = cx; sy = cy; break;
    case 'z': addEnemy(E_ZOMBIE, cx, cy); break;
    case 'd': addEnemy(E_DEMON, cx, cy); break;
    case 'f': addEnemy(E_CASTER, cx, cy); break;
    case 'k': addEnemy(E_BOSS, cx, cy); break;
    case 'h': addItem(I_HEALTH, cx, cy); break;
    case 'a': addItem(I_ARMOR, cx, cy); break;
    case 'm': addItem(I_BULLETS, cx, cy); break;
    case 'e': addItem(I_SHELLS, cx, cy); break;
    case 's': addItem(I_SHOTGUN, cx, cy); break;
    case 'g': addItem(I_MG, cx, cy); break;
    case 'r': addItem(I_KEY_R, cx, cy); break;
    case 'y': addItem(I_KEY_Y, cx, cy); break;
    case 'b': addItem(I_KEY_B, cx, cy); break;
    case 't': addItem(I_TORCH, cx, cy); break;
    case 'c': addItem(I_CORPSE, cx, cy); break;
    default: break;
  }
}

static uint16_t parseColor(const char* v, uint16_t def) {
  int c[3];
  return parseInts(v, c, 3) == 3 ? rgb(c[0], c[1], c[2]) : def;
}

static void updateCamera() {
  w->dirX = cosf(w->pa);  w->dirY = sinf(w->pa);
  w->plX = -w->dirY * 0.66f;  w->plY = w->dirX * 0.66f;
}

static bool loadLevel(int idx) {
  bool owned;
  const char* txt = w->levels.load(idx, &owned);
  if (!txt) return false;
  memset(w->grid, 1, sizeof(w->grid));
  w->nDoors = w->nEn = w->nIt = 0;
  w->totalKills = w->totalSecrets = w->kills = w->secrets = 0;
  memset(w->pr, 0, sizeof(w->pr));
  memset(w->puffs, 0, sizeof(w->puffs));
  memset(w->drops, 0, sizeof(w->drops));
  snprintf(w->name, sizeof(w->name), "%s", w->levels.title[idx]);
  w->par = 120; w->light = 200; w->fog = 45; w->flicker = false;
  w->fogColor = rgb(40, 0, 0); w->ceilColor = rgb(40, 28, 28); w->floorColor = rgb(70, 40, 32);
  float angle = 0, sx = -1, sy = -1;
  int row = 0, mw = 0;
  TextReader r(txt);
  while (r.next()) {
    if (!r.section[0]) {
      const char *k, *v;
      if (!r.kv(&k, &v)) continue;
      if (streq(k, "par")) w->par = toInt(v, 120);
      else if (streq(k, "light")) w->light = toInt(v, 200);
      else if (streq(k, "fog")) w->fog = toInt(v, 45);
      else if (streq(k, "flicker")) w->flicker = toInt(v) != 0;
      else if (streq(k, "angle")) angle = toFloat(v) * PI_F / 180.0f;
      else if (streq(k, "fogcolor")) w->fogColor = parseColor(v, w->fogColor);
      else if (streq(k, "ceil")) w->ceilColor = parseColor(v, w->ceilColor);
      else if (streq(k, "floor")) w->floorColor = parseColor(v, w->floorColor);
    } else if (r.in("map") && row < MAXM) {
      int n = (int)strlen(r.line);
      if (n > MAXM) n = MAXM;
      if (n > mw) mw = n;
      for (int x = 0; x < n; x++) parseCell(x, row, r.line[x], sx, sy);
      row++;
    }
  }
  w->levels.release(txt, owned);
  w->mw = mw; w->mh = row;
  w->nTorch = 0;
  for (int i = 0; i < w->nIt && w->nTorch < 12; i++)
    if (w->it[i].kind == I_TORCH) { w->torchX[w->nTorch] = w->it[i].x; w->torchY[w->nTorch++] = w->it[i].y; }
  if (mw < 3 || row < 3 || sx < 0) { hwLog("Doom: bolum hatali (%d)", idx); return false; }

  // gizli kapılar komşu duvarın dokusunu taşır
  for (int i = 0; i < w->nDoors; i++) {
    Door& d = w->doors[i];
    if (!d.secret) continue;
    d.tex = T_BRICK;
    static const int8_t N[4][2] = { {1, 0}, {-1, 0}, {0, 1}, {0, -1} };
    for (auto& n : N) {
      uint8_t c = g(d.x + n[0], d.y + n[1]);
      if (c >= 1 && c <= 6) { d.tex = (uint8_t)(c - 1); break; }
    }
  }
  w->px = sx; w->py = sy; w->pa = angle;
  updateCamera();
  w->keys = 0;
  w->fireCd = 0.3f; w->reloadT = w->switchT = w->flashT = w->hurtT = w->lightBoost = 0;
  w->reloading = false; w->levelTime = 0; w->msgT = 0; w->flowT = 0; w->noiseT = 0;
  w->lastReloadPress = -10; w->flick = 1; w->flickT = 0; w->camH = 0.5f;
  w->levelIdx = idx;
  w->startInv = w->inv;
  hwLog("Doom: %s %dx%d dusman %d esya %d kapi %d | heap %u", w->name, mw, row, w->nEn, w->nIt,
        w->nDoors, (unsigned)hwFreeHeap());
  return true;
}

static void freshInventory() {
  Inventory& v = w->inv;
  memset(&v, 0, sizeof(v));
  v.hp = 100; v.armor = 0; v.owned = 1 << W_PISTOL; v.weapon = W_PISTOL;
  v.mag[W_PISTOL] = 12; v.res[W_PISTOL] = 36;
}

static void startLevel(int idx, bool fresh) {
  if (fresh) freshInventory();
  if (!loadLevel(idx)) {
    message("BOLUM YUKLENEMEDI", C_RED);
    w->mode = MD_SELECT;
    return;
  }
  w->mode = MD_PLAY;
  w->modeT = 0;
  message(w->name, rgb(255, 80, 60));
  input::clear();
}

// ---------- silahlar ----------
static bool startReload() {
  Inventory& v = w->inv;
  const WInfo& wi = WEAPONS[v.weapon];
  if (w->reloading || w->switchT > 0) return false;
  if (v.mag[v.weapon] >= wi.mag || v.res[v.weapon] <= 0) return false;
  w->reloading = true;
  w->reloadT = wi.reload;
  return true;
}

static void finishReload() {
  Inventory& v = w->inv;
  int take = WEAPONS[v.weapon].mag - v.mag[v.weapon];
  if (take > v.res[v.weapon]) take = v.res[v.weapon];
  v.mag[v.weapon] += take;
  v.res[v.weapon] -= take;
  w->reloading = false;
}

static void switchTo(uint8_t wp) {
  if (wp == w->inv.weapon && w->switchT <= 0) return;
  w->reloading = false;
  w->inv.weapon = wp;
  w->switchT = 0.35f;
}

static void switchNext() {
  for (int i = 1; i <= W_COUNT; i++) {
    uint8_t n = (uint8_t)((w->inv.weapon + i) % W_COUNT);
    if (w->inv.owned & (1 << n)) { switchTo(n); return; }
  }
}

static void addPuff(float x, float y, bool blood) {
  for (Puff& p : w->puffs)
    if (!p.alive) { p.x = x; p.y = y; p.t = 0.25f; p.blood = blood; p.alive = true; return; }
}

static void dropItem(Enemy& e) {
  int r = irand(0, 99);
  if (e.type == E_ZOMBIE && r < 45) addItem(I_BULLETS, e.x, e.y);
  else if (e.type == E_CASTER && r < 40) addItem(I_SHELLS, e.x, e.y);
  else if (e.type == E_DEMON && r < 15) addItem(I_HEALTH, e.x, e.y);
}

static void wake(Enemy& e) {
  if (e.st == ES_IDLE) { e.st = ES_CHASE; e.cd = 0.6f + frand(); }
}

static void damageEnemy(Enemy& e, int dmg) {
  if (e.st == ES_DYING || e.st == ES_DEAD) return;
  e.hp -= (int16_t)dmg;
  e.flash = 0.12f;
  wake(e);
  if (e.hp <= 0) {
    e.st = ES_DYING;
    e.t = e.type == E_BOSS ? 1.6f : 0.55f;
    w->kills++;
    dropItem(e);
    if (e.type == E_BOSS) message("KASAP LORD OLDU!", rgb(255, 200, 40));
    return;
  }
  if (e.st != ES_WINDUP && frand() < ETYPES[e.type].pain) { e.st = ES_PAIN; e.t = 0.22f; }
}

static void fireWeapon() {
  Inventory& v = w->inv;
  const WInfo& wi = WEAPONS[v.weapon];
  v.mag[v.weapon]--;
  w->fireCd = wi.rate;
  w->flashT = 0.07f;
  w->lightBoost = 0.12f;
  w->noiseT = 0.3f;
  for (int p = 0; p < wi.pellets; p++) {
    float off = (frand() - 0.5f) * 2.0f * wi.spread;
    int col = SCR_W / 2 + (int)(tanf(off) / 0.66f * (SCR_W / 2));
    if (col < 0) col = 0;
    if (col >= SCR_W) col = SCR_W - 1;
    int best = -1;
    float bd = w->zbuf[col];
    for (int i = 0; i < w->nEn; i++) {
      Enemy& e = w->en[i];
      if (e.st == ES_DYING || e.st == ES_DEAD || e.scrW <= 0) continue;
      int tol = e.scrW * 7 / 20;
      if (tol < 2) tol = 2;
      if (abs(e.scrX - col) <= tol && e.depth < bd) { bd = e.depth; best = i; }
    }
    if (best >= 0) {
      Enemy& e = w->en[best];
      int dmg = irand(wi.dmin, wi.dmax);
      if (v.weapon == W_SHOTGUN) {
        float f = 1.25f - e.depth * 0.12f;
        dmg = (int)(dmg * (f < 0.35f ? 0.35f : f > 1.0f ? 1.0f : f));
      }
      damageEnemy(e, dmg);
      addPuff(e.x - w->dirX * 0.2f, e.y - w->dirY * 0.2f, true);
    } else if (p == 0) {
      float camX = 2.0f * col / SCR_W - 1.0f;
      float d = w->zbuf[col] - 0.08f;
      addPuff(w->px + (w->dirX + w->plX * camX) * d, w->py + (w->dirY + w->plY * camX) * d, false);
    }
  }
  if (v.mag[v.weapon] == 0) startReload();
}

// ---------- oyuncu hasarı ----------
static void hurtPlayer(int dmg) {
  if (w->mode != MD_PLAY) return;
  Inventory& v = w->inv;
  int saved = v.armor > 0 ? dmg / 3 : 0;
  if (saved > v.armor) saved = v.armor;
  v.armor -= saved;
  v.hp -= dmg - saved;
  w->hurtT = 0.45f;
  for (int i = 0, n = 0; i < MAX_DROP && n < 2; i++) {
    Drop& d = w->drops[i];
    if (d.life > 0) continue;
    d.x = (float)irand(6, SCR_W - 6); d.y = (float)irand(2, VIEW_H / 2);
    d.life = 0.8f + frand() * 0.6f; d.r = (uint8_t)irand(1, 2);
    n++;
  }
  if (v.hp <= 0) {
    v.hp = 0;
    w->mode = MD_DEAD;
    w->modeT = 0;
    input::clear();
  }
}

// ---------- kapılar ----------
static bool occupied(int cx, int cy) {
  if ((int)w->px == cx && (int)w->py == cy) return true;
  for (int i = 0; i < w->nEn; i++) {
    Enemy& e = w->en[i];
    if (e.st != ES_DEAD && e.st != ES_DYING && (int)e.x == cx && (int)e.y == cy) return true;
  }
  return false;
}

static void updateDoors(float dt) {
  for (int i = 0; i < w->nDoors; i++) {
    Door& d = w->doors[i];
    if (d.dir > 0) {
      d.open += dt * 2.2f;
      if (d.open >= 1) { d.open = 1; d.dir = 0; d.wait = 4.0f; }
    } else if (d.dir < 0) {
      if (occupied(d.x, d.y)) d.dir = 1;
      d.open -= dt * 2.2f;
      if (d.open <= 0) { d.open = 0; d.dir = 0; }
    } else if (d.open >= 1 && !d.secret) {
      d.wait -= dt;
      if (d.wait <= 0) {
        if (occupied(d.x, d.y)) d.wait = 1.0f;
        else d.dir = -1;
      }
    }
  }
}

static const char* const KEY_NAMES[] = { "", "KIRMIZI", "SARI", "MAVI" };
static const uint16_t KEY_COLORS[] = { 0, rgb(230, 30, 30), rgb(240, 210, 0), rgb(40, 90, 255) };

static void useFront() {
  int fx = (int)(w->px + w->dirX * 0.8f), fy = (int)(w->py + w->dirY * 0.8f);
  uint8_t c = g(fx, fy);
  if (c == 7) {   // çıkış
    w->mode = MD_STATS;
    w->modeT = 0;
    input::clear();
    if (w->levelIdx + 2 > w->unlocked) {
      w->unlocked = w->levelIdx + 2;
      save::set("dm_unl", w->unlocked);
    }
    char key[16];
    save::keyFor(key, "dmt", w->levels.title[w->levelIdx]);
    save::setMin(key, (int32_t)(w->levelTime * 1000));
    return;
  }
  if (!(c & 0x80)) return;
  Door& d = w->doors[c & 0x7F];
  if (d.open > 0 && d.dir >= 0) return;
  if (d.key && !(w->keys & (1 << d.key))) {
    if (w->msgT <= 0.5f) {
      char m[28];
      snprintf(m, sizeof(m), "%s ANAHTAR GEREKLI", KEY_NAMES[d.key]);
      message(m, KEY_COLORS[d.key]);
    }
    return;
  }
  if (d.secret && d.open == 0 && d.dir == 0) {
    w->secrets++;
    message("GIZLI ODA!", rgb(120, 255, 120));
  }
  d.dir = 1;
}

// ---------- eşya toplama ----------
static void pickup(Item& it) {
  Inventory& v = w->inv;
  switch (it.kind) {
    case I_HEALTH:
      if (v.hp >= 100) return;
      v.hp = v.hp + 25 > 100 ? 100 : v.hp + 25;
      message("+25 CAN", rgb(255, 90, 90));
      break;
    case I_ARMOR:
      if (v.armor >= 100) return;
      v.armor = v.armor + 50 > 100 ? 100 : v.armor + 50;
      message("+50 ZIRH", rgb(90, 255, 90));
      break;
    case I_BULLETS:
      if (v.res[W_PISTOL] >= WEAPONS[W_PISTOL].maxRes && v.res[W_MG] >= WEAPONS[W_MG].maxRes) return;
      v.res[W_PISTOL] += 20;
      v.res[W_MG] += 40;
      message("MERMI", rgb(255, 220, 90));
      break;
    case I_SHELLS:
      if (v.res[W_SHOTGUN] >= WEAPONS[W_SHOTGUN].maxRes) return;
      v.res[W_SHOTGUN] += 8;
      message("FISEK", rgb(255, 220, 90));
      break;
    case I_SHOTGUN:
    case I_MG: {
      uint8_t wp = it.kind == I_SHOTGUN ? W_SHOTGUN : W_MG;
      if (!(v.owned & (1 << wp))) {
        v.owned |= (uint8_t)(1 << wp);
        v.mag[wp] = WEAPONS[wp].mag;
        switchTo(wp);
        message(wp == W_SHOTGUN ? "POMPALI ALINDI!" : "MAKINELI ALINDI!", rgb(255, 200, 40));
      } else {
        message(WEAPONS[wp].name, rgb(255, 220, 90));
      }
      v.res[wp] += wp == W_SHOTGUN ? 8 : 40;
      break;
    }
    case I_KEY_R: case I_KEY_Y: case I_KEY_B: {
      int k = it.kind - I_KEY_R + 1;
      w->keys |= (uint8_t)(1 << k);
      char m[28];
      snprintf(m, sizeof(m), "%s ANAHTAR", KEY_NAMES[k]);
      message(m, KEY_COLORS[k]);
      break;
    }
    default: return;
  }
  for (int i = 0; i < W_COUNT; i++)
    if (v.res[i] > WEAPONS[i].maxRes) v.res[i] = WEAPONS[i].maxRes;
  it.gone = true;
}

// ---------- düşman yapay zekası ----------
static void buildFlow() {
  memset(w->flow, 255, sizeof(w->flow));
  int sx = (int)w->px, sy = (int)w->py;
  int head = 0, tail = 0;
  w->flow[sy * MAXM + sx] = 0;
  w->queue[tail++] = (uint16_t)(sy * MAXM + sx);
  while (head < tail) {
    int c = w->queue[head++];
    int cx = c % MAXM, cy = c / MAXM;
    uint8_t dist = w->flow[c];
    if (dist >= 254) continue;
    static const int8_t N[4][2] = { {1, 0}, {-1, 0}, {0, 1}, {0, -1} };
    for (auto& n : N) {
      int nx = cx + n[0], ny = cy + n[1];
      if (nx < 0 || ny < 0 || nx >= w->mw || ny >= w->mh) continue;
      int ni = ny * MAXM + nx;
      if (w->flow[ni] != 255) continue;
      uint8_t cell = w->grid[ni];
      bool pass = cell == 0;
      if (cell & 0x80) {
        Door& d = w->doors[cell & 0x7F];
        pass = (!d.key && !d.secret) || d.open > 0.85f;
      }
      if (!pass) continue;
      w->flow[ni] = (uint8_t)(dist + 1);
      w->queue[tail++] = (uint16_t)ni;
    }
  }
}

static void moveEnemy(Enemy& e, float tx, float ty, float speed, float dt) {
  float dx = tx - e.x, dy = ty - e.y;
  float d = sqrtf(dx * dx + dy * dy);
  if (d < 0.01f) return;
  float r = ETYPES[e.type].radius;
  float s = speed * dt;
  float ox = e.x, oy = e.y;
  tryMove(e.x, e.y, dx / d * s, dy / d * s, r);
  // önündeki sıradan kapıyı aç
  int fx = (int)(e.x + dx / d * (r + 0.2f)), fy = (int)(e.y + dy / d * (r + 0.2f));
  uint8_t c = g(fx, fy);
  if (c & 0x80) {
    Door& door = w->doors[c & 0x7F];
    if (!door.key && !door.secret && door.open < 1 && door.dir <= 0) door.dir = 1;
  }
  if (fabsf(e.x - ox) + fabsf(e.y - oy) < s * 0.2f) e.strafeT = 0;   // takıldıysa yön değiştir
}

static void chaseStep(Enemy& e, float dt, float speed) {
  float tx = w->px, ty = w->py;
  if (!e.sees) {
    // akış alanında komşu hücrelerden en yakınına git
    int cx = (int)e.x, cy = (int)e.y;
    uint8_t best = w->flow[cy * MAXM + cx];
    static const int8_t N[4][2] = { {1, 0}, {-1, 0}, {0, 1}, {0, -1} };
    for (auto& n : N) {
      int nx = cx + n[0], ny = cy + n[1];
      if (nx < 0 || ny < 0 || nx >= w->mw || ny >= w->mh) continue;
      uint8_t f = w->flow[ny * MAXM + nx];
      if (f < best) { best = f; tx = nx + 0.5f; ty = ny + 0.5f; }
    }
    if (best == 255) return;
  }
  moveEnemy(e, tx, ty, speed, dt);
}

static void enemyAttack(Enemy& e, const ETypeInfo& ti, float dist) {
  if (!ti.ranged || (e.type == E_BOSS && dist < ti.range)) {
    if (dist < ti.range + 0.35f) hurtPlayer(irand(ti.dmin, ti.dmax));
    return;
  }
  float dx = w->px - e.x, dy = w->py - e.y;
  float base = atan2f(dy, dx);
  int shots = e.type == E_BOSS ? 3 : 1;
  for (int s = 0; s < shots; s++) {
    float a = base + (s - (shots - 1) * 0.5f) * 0.22f + (frand() - 0.5f) * 0.06f;
    for (Proj& p : w->pr) {
      if (p.alive) continue;
      float sp = e.type == E_BOSS ? 6.0f : 5.0f;
      p.x = e.x + cosf(a) * 0.4f; p.y = e.y + sinf(a) * 0.4f;
      p.dx = cosf(a) * sp; p.dy = sinf(a) * sp;
      p.dmg = (int16_t)irand(e.type == E_BOSS ? 15 : ti.dmin, e.type == E_BOSS ? 24 : ti.dmax);
      p.life = 4.0f; p.alive = true; p.boss = e.type == E_BOSS;
      break;
    }
  }
}

static void updateEnemies(float dt) {
  w->flowT -= dt;
  if (w->flowT <= 0) { buildFlow(); w->flowT = 0.3f; }
  for (int i = 0; i < w->nEn; i++) {
    Enemy& e = w->en[i];
    const ETypeInfo& ti = ETYPES[e.type];
    if (e.flash > 0) e.flash -= dt;
    if (e.st == ES_DEAD) continue;
    if (e.st == ES_DYING) { e.t -= dt; if (e.t <= 0) e.st = ES_DEAD; continue; }
    float dx = w->px - e.x, dy = w->py - e.y;
    float dist = sqrtf(dx * dx + dy * dy);
    e.seeT -= dt;
    if (e.seeT <= 0) {
      e.sees = dist < 16 && lineOfSight(e.x, e.y, w->px, w->py);
      e.seeT = 0.2f;
    }
    e.cd -= dt;
    bool enraged = e.type == E_BOSS && e.hp < ti.hp / 2;
    float speed = ti.speed * (enraged ? 1.5f : 1.0f);
    switch (e.st) {
      case ES_IDLE:
        if ((e.sees && dist < 14) || (w->noiseT > 0 && dist < 9)) wake(e);
        break;
      case ES_CHASE: {
        e.anim += dt * speed * 2.2f;
        bool melee = !ti.ranged || (e.type == E_BOSS && dist < ti.range);
        if (melee && dist < ti.range) { e.st = ES_WINDUP; e.t = ti.windup; break; }
        if (ti.ranged && e.sees && dist < 10 && e.cd <= 0 && !(e.type == E_BOSS && dist < ti.range)) {
          e.st = ES_WINDUP; e.t = ti.windup * (enraged ? 0.6f : 1.0f); break;
        }
        if (e.type == E_CASTER && e.sees && dist < 6) {
          // mesafeyi koru: yakınsa geri çekil, değilse yana kay
          e.strafeT -= dt;
          if (e.strafeT <= 0) { e.strafe = (int8_t)-e.strafe; e.strafeT = 1.2f + frand(); }
          float nx = -dy / dist * e.strafe, ny = dx / dist * e.strafe;
          float back = dist < 3.5f ? -1.0f : 0.0f;
          moveEnemy(e, e.x + nx + dx / dist * back, e.y + ny + dy / dist * back, speed, dt);
        } else if (e.type == E_DEMON && e.sees && dist > 1.5f) {
          float zig = sinf(e.anim * 2.5f) * 0.8f;
          moveEnemy(e, w->px - dy / dist * zig, w->py + dx / dist * zig, speed, dt);
        } else {
          chaseStep(e, dt, speed);
        }
        break;
      }
      case ES_WINDUP:
        e.t -= dt;
        if (e.t <= 0) {
          enemyAttack(e, ti, dist);
          e.cd = ti.cooldown * (enraged ? 0.6f : 1.0f) * (0.8f + frand() * 0.4f);
          e.st = ES_CHASE;
        }
        break;
      case ES_PAIN:
        e.t -= dt;
        if (e.t <= 0) e.st = ES_CHASE;
        break;
      default: break;
    }
  }
  // birbirlerinin içine girmesinler
  for (int i = 0; i < w->nEn; i++) {
    Enemy& a = w->en[i];
    if (a.st >= ES_DYING) continue;
    for (int j = i + 1; j < w->nEn; j++) {
      Enemy& b = w->en[j];
      if (b.st >= ES_DYING) continue;
      float dx = b.x - a.x, dy = b.y - a.y;
      float min = ETYPES[a.type].radius + ETYPES[b.type].radius;
      float d2 = dx * dx + dy * dy;
      if (d2 >= min * min || d2 < 1e-6f) continue;
      float d = sqrtf(d2), push = (min - d) * 0.5f;
      tryMove(a.x, a.y, -dx / d * push, -dy / d * push, ETYPES[a.type].radius);
      tryMove(b.x, b.y, dx / d * push, dy / d * push, ETYPES[b.type].radius);
    }
  }
}

static void updateProjectiles(float dt) {
  for (Proj& p : w->pr) {
    if (!p.alive) continue;
    p.life -= dt;
    p.x += p.dx * dt;
    p.y += p.dy * dt;
    float dx = p.x - w->px, dy = p.y - w->py;
    if (dx * dx + dy * dy < 0.35f * 0.35f) { hurtPlayer(p.dmg); p.alive = false; addPuff(p.x, p.y, false); continue; }
    if (solid((int)p.x, (int)p.y) || p.life <= 0) { p.alive = false; addPuff(p.x - p.dx * 0.03f, p.y - p.dy * 0.03f, false); }
  }
}

// ---------- oyuncu ----------
static void updatePlayer(float dt) {
  Inventory& v = w->inv;
  const float MOVE = 2.8f, ROT = 2.5f;
  if (input::down(B_LEFT))  w->pa -= ROT * dt;
  if (input::down(B_RIGHT)) w->pa += ROT * dt;
  updateCamera();
  float mv = 0;
  if (input::down(B_FWD))  mv += MOVE * dt;
  if (input::down(B_BACK)) mv -= MOVE * dt * 0.75f;
  w->walking = mv != 0;
  if (w->walking) { w->bobT += dt * 9.0f; tryMove(w->px, w->py, w->dirX * mv, w->dirY * mv, 0.22f); }
  if (input::down(B_FWD)) useFront();
  if (w->mode != MD_PLAY) return;

  // RELOAD: tek basış elle reload, 0.3 sn içinde ikinci basış -> reload iptal + sonraki silah
  if (input::pressed(B_RELOAD)) {
    if (w->levelTime - w->lastReloadPress < 0.3f) {
      w->reloading = false;
      switchNext();
      w->lastReloadPress = -10;
    } else {
      startReload();
      w->lastReloadPress = w->levelTime;
    }
  }
  w->fireCd -= dt;
  if (w->switchT > 0) w->switchT -= dt;
  if (w->reloading) { w->reloadT -= dt; if (w->reloadT <= 0) finishReload(); }
  if (w->emptyMsgT > 0) w->emptyMsgT -= dt;
  if (input::down(B_FIRE) && w->fireCd <= 0 && !w->reloading && w->switchT <= 0) {
    if (v.mag[v.weapon] > 0) fireWeapon();
    else if (v.res[v.weapon] > 0) startReload();
    else {
      // bu silahta mermi kalmadı: mermisi olan silaha geç
      for (int i = W_COUNT - 1; i >= 0; i--)
        if ((v.owned & (1 << i)) && (v.mag[i] > 0 || v.res[i] > 0)) { switchTo((uint8_t)i); break; }
      if (w->emptyMsgT <= 0) { message("MERMI YOK", C_RED); w->emptyMsgT = 1.5f; }
      w->fireCd = 0.3f;
    }
  }
  for (int i = 0; i < w->nIt; i++) {
    Item& it = w->it[i];
    if (it.gone || it.kind >= I_TORCH) continue;
    float dx = it.x - w->px, dy = it.y - w->py;
    if (dx * dx + dy * dy < 0.5f * 0.5f) pickup(it);
  }
}

static void updateFlicker(float dt) {
  if (!w->flicker) { w->flick = 1; return; }
  w->flickT -= dt;
  if (w->flickT <= 0) {
    if (frand() < 0.18f) { w->flickTarget = 0.3f + frand() * 0.3f; w->flickT = 0.05f + frand() * 0.12f; }
    else { w->flickTarget = 0.92f + frand() * 0.08f; w->flickT = 0.1f + frand() * 0.9f; }
  }
  w->flick += (w->flickTarget - w->flick) * (dt * 30 > 1 ? 1 : dt * 30);
}

static void update(float dt) {
  w->modeT += dt;
  if (w->msgT > 0) w->msgT -= dt;
  switch (w->mode) {
    case MD_SELECT: {
      int r = w->list.update(w->levels.count, dt, 6);
      if (r == -2) app::requestMenu();
      else if (r >= 0) {
        if (r < w->unlocked) startLevel(r, true);
        else message("KILITLI", C_RED);
      }
      return;
    }
    case MD_DEAD:
      if (w->camH > 0.12f) w->camH -= dt * 0.5f;
      updateEnemies(dt);
      updateProjectiles(dt);
      if (w->modeT > 1.2f) {
        if (input::pressed(B_FIRE)) { w->inv = w->startInv; startLevel(w->levelIdx, false); }
        else if (input::pressed(B_RELOAD)) { w->mode = MD_SELECT; input::clear(); }
      }
      return;
    case MD_STATS:
      if (w->modeT > 0.8f && input::pressed(B_FIRE)) {
        if (w->levelIdx + 1 < w->levels.count) {
          startLevel(w->levelIdx + 1, false);   // envanter sonraki bölüme taşınır
        } else {
          w->mode = MD_VICTORY;
          w->modeT = 0;
          input::clear();
        }
      } else if (input::pressed(B_RELOAD)) { w->mode = MD_SELECT; input::clear(); }
      return;
    case MD_VICTORY:
      if (w->modeT > 1.0f && input::pressed(B_FIRE)) { w->mode = MD_SELECT; input::clear(); }
      return;
    case MD_PLAY:
      break;
  }
  w->levelTime += dt;
  if (w->flashT > 0) w->flashT -= dt;
  if (w->hurtT > 0) w->hurtT -= dt;
  if (w->lightBoost > 0) w->lightBoost -= dt;
  if (w->noiseT > 0) w->noiseT -= dt;
  for (Puff& p : w->puffs) if (p.alive && (p.t -= dt) <= 0) p.alive = false;
  for (Drop& d : w->drops) if (d.life > 0) { d.life -= dt * 0.6f; d.y += dt * 9; }
  updateFlicker(dt);
  updatePlayer(dt);
  updateDoors(dt);
  updateEnemies(dt);
  updateProjectiles(dt);
  float bob = w->walking ? sinf(w->bobT) * 0.018f : 0;
  if (w->mode == MD_PLAY) w->camH = 0.5f + bob;
}

// ---------- çizim ----------
// Mesafeye göre parlaklık (0..256) ve sis rengi
static inline int lightAt(float d, int base) {
  float k = w->fog * 0.0012f;
  int f = (int)(base / (1.0f + d * d * k));
  return f > 256 ? 256 : f;
}
static inline uint16_t fogFor(int f) {
  int amt = 256 - f;
  return shade(w->fogColor, amt < 0 ? 0 : amt);
}
// Meşalelerin bir noktaya kattığı ışık
static int torchLight(float x, float y) {
  int add = 0;
  for (int i = 0; i < w->nTorch; i++) {
    float dx = w->torchX[i] - x, dy = w->torchY[i] - y;
    float d2 = dx * dx + dy * dy;
    if (d2 < 9.0f) add += (int)((1.0f - d2 / 9.0f) * (90 + (hash32((uint32_t)(w->levelTime * 15) + i) & 31)));
  }
  return add;
}

static int baseLight() {
  float b = w->light * w->flick * (1.0f + (w->lightBoost > 0 ? 0.6f : 0));
  return (int)b;
}

static void renderWorld() {
  int horizon = VIEW_H / 2;
  if (w->hurtT > 0.25f) horizon += irand(-2, 2);
  w->horizon = horizon;
  float camH = w->camH;
  int base = baseLight();

  // tavan/zemin satır renkleri
  for (int y = 0; y < VIEW_H; y++) {
    float d;
    uint16_t c;
    if (y < horizon) { d = (1.0f - camH) * VIEW_H / (horizon - y + 0.5f); c = w->ceilColor; }
    else             { d = camH * VIEW_H / (y - horizon + 0.5f);          c = w->floorColor; }
    int f = lightAt(d, base) * 3 / 4;
    w->rowColor[y] = sw16(addc(shade(c, f), fogFor(f)));
  }

  for (int x = 0; x < SCR_W; x++) {
    float camX = 2.0f * x / SCR_W - 1.0f;
    float rdx = w->dirX + w->plX * camX, rdy = w->dirY + w->plY * camX;
    int mx = (int)w->px, my = (int)w->py;
    float ddx = (rdx == 0) ? 1e30f : fabsf(1.0f / rdx);
    float ddy = (rdy == 0) ? 1e30f : fabsf(1.0f / rdy);
    int stepX, stepY; float sdx, sdy;
    if (rdx < 0) { stepX = -1; sdx = (w->px - mx) * ddx; } else { stepX = 1; sdx = (mx + 1.0f - w->px) * ddx; }
    if (rdy < 0) { stepY = -1; sdy = (w->py - my) * ddy; } else { stepY = 1; sdy = (my + 1.0f - w->py) * ddy; }

    struct Hit { float dist; int texX; uint8_t tex; int side; float open; };
    Hit hits[3];
    int nh = 0;
    for (int i = 0; i < 64; i++) {
      int side;
      if (sdx < sdy) { sdx += ddx; mx += stepX; side = 0; }
      else           { sdy += ddy; my += stepY; side = 1; }
      uint8_t c = g(mx, my);
      if (!c) continue;
      float open = 0;
      uint8_t tx;
      if (c & 0x80) {
        const Door& d = w->doors[c & 0x7F];
        if (d.open >= 0.99f) continue;
        open = d.open;
        tx = d.tex;
      } else {
        tx = (uint8_t)(c - 1);
      }
      float perp = side == 0 ? (sdx - ddx) : (sdy - ddy);
      if (perp < 0.05f) perp = 0.05f;
      float wallX = side == 0 ? w->py + perp * rdy : w->px + perp * rdx;
      wallX -= floorf(wallX);
      int texX = (int)(wallX * TEX) & (TEX - 1);
      if (side == 0 && rdx < 0) texX = TEX - 1 - texX;   // dokular her yüzden düz görünsün
      if (side == 1 && rdy > 0) texX = TEX - 1 - texX;
      hits[nh++] = { perp, texX, tx, side, open };
      if (open <= 0.01f || nh == 3) { hits[nh - 1].open = 0; break; }
    }
    if (nh == 0) hits[nh++] = { 64.0f, 0, T_BRICK, 0, 0 };
    hits[nh - 1].open = 0;   // en arkadaki yüzey her zaman tam çizilir

    uint16_t* col = fb + x;
    // en uzaktan yakına: önce arkadaki duvar, sonra yarı açık kapılar
    for (int h = nh - 1; h >= 0; h--) {
      const Hit& hit = hits[h];
      float perp = hit.dist;
      int lineH = (int)(VIEW_H / perp);
      if (lineH < 1) lineH = 1;
      int top = horizon - (int)(lineH * (1.0f - camH));
      int bottom = top + lineH;
      int visBottom = hit.open > 0 ? top + (int)(lineH * (1.0f - hit.open)) : bottom;
      int y0 = top < 0 ? 0 : top;
      int y1 = visBottom > VIEW_H ? VIEW_H : visBottom;
      // ışık
      float hx = w->px + rdx * perp, hy = w->py + rdy * perp;
      int f = lightAt(perp, base + torchLight(hx, hy));
      if (hit.side) f = f * 3 / 4;
      uint16_t fogC = fogFor(f);
      if (h == nh - 1) {
        for (int y = 0; y < y0 && y < VIEW_H; y++) col[y * SCR_W] = w->rowColor[y];
        int fy = bottom < 0 ? 0 : bottom;
        for (int y = fy; y < VIEW_H; y++) col[y * SCR_W] = w->rowColor[y];
      }
      const uint16_t* t = w->tex + hit.tex * TEX * TEX;
      uint32_t step = ((uint32_t)TEX << 16) / (uint32_t)lineH;
      uint32_t tp = (uint32_t)(y0 - top) * step + (uint32_t)(hit.open * TEX * 65536.0f);
      for (int y = y0; y < y1; y++) {
        int ty = (tp >> 16) & (TEX - 1);
        tp += step;
        col[y * SCR_W] = sw16(addc(shade(t[ty * TEX + hit.texX], f), fogC));
      }
    }
    w->zbuf[x] = (nh > 1 && hits[0].open < 0.5f) ? hits[0].dist : hits[nh - 1].dist;
  }
}

// Ölçeklenmiş ASCII sprite'ı z-buffer'a göre çizer
static void drawArt3D(const Art& a, int cx, int bottom, int sw, int sh, float depth, bool flip,
                      int f, uint16_t fogC, int flashAmt, uint16_t tintTo = 0, int tint = 0) {
  if (sw < 1 || sh < 1) return;
  int x0 = cx - sw / 2, top = bottom - sh;
  int xa = x0 < 0 ? 0 : x0, xb = x0 + sw > SCR_W ? SCR_W : x0 + sw;
  int ya = top < 0 ? 0 : top, yb = bottom > VIEW_H ? VIEW_H : bottom;
  for (int x = xa; x < xb; x++) {
    if (depth >= w->zbuf[x]) continue;
    int u = (x - x0) * a.w / sw;
    if (flip) u = a.w - 1 - u;
    for (int y = ya; y < yb; y++) {
      int v = (y - top) * a.h / sh;
      char ch = a.rows[v][u];
      if (ch == '.') continue;
      uint16_t c = w->pal[(uint8_t)ch & 127];
      if (!fullbright(ch)) c = addc(shade(c, f), fogC);
      if (tint) c = mix(c, tintTo, tint);
      if (flashAmt) c = mix(c, C_WHITE, flashAmt);
      fb[y * SCR_W + x] = sw16(c);
    }
  }
}

static void drawGlow(int cx, int cy, int r, float depth, uint16_t outer, uint16_t inner) {
  if (cx < 0 || cx >= SCR_W || depth >= w->zbuf[cx]) return;
  gfx::fillCircle(cx, cy, r, outer);
  gfx::fillCircle(cx, cy, r * 2 / 3, inner);
  if (r > 2) gfx::fillCircle(cx, cy, r / 3, C_WHITE);
}

static void renderSprites() {
  float invDet = 1.0f / (w->plX * w->dirY - w->dirX * w->plY);
  int n = 0;
  auto add = [&](float x, float y, VisKind k, int idx) {
    float sx = x - w->px, sy = y - w->py;
    float ty = invDet * (-w->plY * sx + w->plX * sy);
    if (ty <= 0.15f || n >= MAX_VIS) return;
    w->vis[n++] = { ty, invDet * (w->dirY * sx - w->dirX * sy), (uint8_t)k, (uint8_t)idx };
  };
  for (int i = 0; i < w->nEn; i++) { w->en[i].scrW = 0; add(w->en[i].x, w->en[i].y, V_ENEMY, i); }
  for (int i = 0; i < w->nIt; i++) if (!w->it[i].gone) add(w->it[i].x, w->it[i].y, V_ITEM, i);
  for (int i = 0; i < MAX_PR; i++) if (w->pr[i].alive) add(w->pr[i].x, w->pr[i].y, V_PROJ, i);
  for (int i = 0; i < MAX_PUFF; i++) if (w->puffs[i].alive) add(w->puffs[i].x, w->puffs[i].y, V_PUFF, i);
  for (int i = 1; i < n; i++) {   // uzaktan yakına
    Vis k = w->vis[i];
    int j = i - 1;
    while (j >= 0 && w->vis[j].depth < k.depth) { w->vis[j + 1] = w->vis[j]; j--; }
    w->vis[j + 1] = k;
  }
  int base = baseLight();
  for (int i = 0; i < n; i++) {
    const Vis& v = w->vis[i];
    float ty = v.depth;
    int cx = (int)((SCR_W / 2) * (1.0f + v.tx / ty));
    float unit = VIEW_H / ty;                         // 1 dünya birimi kaç piksel
    int floorY = w->horizon + (int)(unit * w->camH);
    if (v.kind == V_ENEMY) {
      Enemy& e = w->en[v.idx];
      const ETypeInfo& ti = ETYPES[e.type];
      int f = lightAt(ty, base + torchLight(e.x, e.y));
      uint16_t fogC = fogFor(f);
      ArtId id = ti.walk;
      bool flip = false;
      int sh = (int)(unit * ti.height);
      int bottom = floorY - (int)(unit * ti.lift * (1.0f + sinf(w->levelTime * 3 + v.idx) * 0.3f));
      int tint = 0;
      if (e.st == ES_DEAD) {
        id = ti.dead;
        bottom = floorY;
      } else if (e.st == ES_DYING) {
        float total = e.type == E_BOSS ? 1.6f : 0.55f;
        float p = 1.0f - e.t / total;               // 0 -> 1
        if (p > 0.7f) { id = ti.dead; bottom = floorY; }
        else { sh = (int)(sh * (1.0f - p * 0.9f)); bottom = floorY; tint = (int)(p * 300); }
        if (e.type == E_BOSS) cx += irand(-2, 2);
      } else if (e.st == ES_WINDUP) {
        id = ti.atk;
      } else if (e.st == ES_PAIN) {
        cx += irand(-2, 2);
        bottom += irand(-1, 1);
      } else if (e.st == ES_CHASE) {
        flip = ((int)e.anim) & 1;
        bottom -= ((int)(e.anim * 2) & 1);
      }
      const Art& a = art(id);
      // ceset karesi yürüme karesiyle aynı ölçekte (piksel başına) çizilir
      if (id == ti.dead) sh = (int)(unit * ti.height * a.h / art(ti.walk).h);
      int swd = id == ti.dead ? (int)(unit * ti.height * a.w / art(ti.walk).h) : sh * a.w / a.h;
      int flash = e.flash > 0 ? 150 : 0;
      drawArt3D(a, cx, bottom, swd, sh, ty, flip, f, fogC, flash, rgb(160, 0, 0), tint > 256 ? 256 : tint);
      if (e.st < ES_DYING) { e.scrX = (int16_t)cx; e.scrW = (int16_t)swd; e.depth = ty; }
    } else if (v.kind == V_ITEM) {
      if (ty < 0.35f) continue;   // içinden geçilen eşya ekranı kaplamasın
      Item& it = w->it[v.idx];
      int f = lightAt(ty, base + torchLight(it.x, it.y));
      uint16_t fogC = fogFor(f);
      ArtId id; float hgt;
      switch (it.kind) {
        case I_HEALTH: id = A_MEDKIT; hgt = 0.26f; break;
        case I_ARMOR: id = A_ARMOR; hgt = 0.3f; break;
        case I_BULLETS: id = A_BULLETS; hgt = 0.2f; break;
        case I_SHELLS: id = A_SHELLS; hgt = 0.2f; break;
        case I_SHOTGUN: id = A_SHOTGUN; hgt = 0.18f; break;
        case I_MG: id = A_MG; hgt = 0.2f; break;
        case I_TORCH: id = A_TORCH; hgt = 0.75f; break;
        case I_CORPSE: id = A_ZOMBIE_DEAD; hgt = 0.27f; break;
        default: id = A_KEY; hgt = 0.18f; break;
      }
      const Art& a = art(id);
      int sh = (int)(unit * hgt);
      int swd = sh * a.w / a.h;
      int bottom = floorY;
      if (it.kind >= I_KEY_R && it.kind <= I_KEY_B) {
        bottom -= (int)(unit * (0.12f + sinf(w->levelTime * 4) * 0.05f));
        uint16_t kc = KEY_COLORS[it.kind - I_KEY_R + 1];
        w->pal['Z'] = kc;
        w->pal['z'] = shade(kc, 120);
        drawArt3D(a, cx, bottom, swd, sh, ty, false, 256, 0, 0);
        w->pal['Z'] = palColor('Z');
        w->pal['z'] = palColor('z');
      } else {
        drawArt3D(a, cx, bottom, swd, sh, ty, false, f, fogC, 0);
      }
      if (it.kind == I_TORCH) {   // titreyen alev
        int fy = bottom - sh;
        int r = (int)(unit * 0.08f) + 1;
        int jit = (int)(hash32((uint32_t)(w->levelTime * 20) + v.idx) & 3);
        drawGlow(cx, fy - r + 1 - jit / 2, r + (jit & 1), ty, rgb(200, 40, 0), rgb(255, 170, 30));
      }
    } else if (v.kind == V_PROJ) {
      const Proj& p = w->pr[v.idx];
      int r = (int)(unit * (p.boss ? 0.13f : 0.1f)) + 1;
      if (r > 14) r = 14;
      int flick = (int)(hash32((uint32_t)(w->levelTime * 30) + v.idx) & 1);
      if (p.boss) drawGlow(cx, w->horizon, r + flick, ty, rgb(40, 180, 20), rgb(180, 255, 80));
      else drawGlow(cx, w->horizon, r + flick, ty, rgb(220, 50, 0), rgb(255, 200, 40));
    } else {
      const Puff& p = w->puffs[v.idx];
      int r = (int)(unit * 0.06f * (1.5f - p.t * 2)) + 1;
      if (r > 5) r = 5;
      if (p.blood) drawGlow(cx, w->horizon + (int)(unit * 0.05f), r, ty, rgb(150, 0, 0), rgb(220, 20, 10));
      else drawGlow(cx, w->horizon, r, ty, rgb(120, 110, 100), rgb(255, 220, 120));
    }
  }
}

// ---------- silah çizimi ----------
static const uint16_t SKIN = 0xC3EE;   // rgb(195,125,115)

// yumruk: avuç, boğumlar ve başparmak
static void hand(int x, int y) {
  uint16_t dark = shade(SKIN, 150);
  gfx::fillRect(x - 6, y - 5, 12, 11, shade(SKIN, 210));
  gfx::fillRect(x - 5, y - 6, 10, 2, SKIN);
  for (int i = 0; i < 3; i++) gfx::vline(x - 3 + i * 3, y - 5, 4, dark);
  gfx::fillRect(x - 8, y - 2, 4, 5, SKIN);
  gfx::hline(x - 6, y + 5, 12, dark);
  gfx::fillRect(x - 5, y + 6, 10, 6, rgb(60, 50, 40));   // eldiven kolu
}

static void muzzle(int x, int y, int r) {
  int j = irand(0, 2);
  gfx::fillCircle(x, y, r + j, rgb(255, 100, 0));
  gfx::fillCircle(x, y, r * 2 / 3, rgb(255, 220, 60));
  gfx::fillCircle(x, y, r / 3, C_WHITE);
  for (int i = 0; i < 4; i++) {
    float a = i * 1.57f + j;
    gfx::line(x, y, x + (int)(cosf(a) * r * 1.6f), y + (int)(sinf(a) * r * 1.6f), rgb(255, 160, 20));
  }
}

static void drawWeapon() {
  Inventory& v = w->inv;
  int bob  = w->walking ? (int)(fabsf(sinf(w->bobT)) * 4) : 0;
  int sway = w->walking ? (int)(cosf(w->bobT * 0.5f) * 3) : 0;
  int gx = SCR_W / 2 + sway, gy = VIEW_H + bob;
  int tilt = 0;
  if (w->switchT > 0) gy += (int)(w->switchT / 0.35f * 45);
  if (w->reloading) {
    float p = 1.0f - w->reloadT / WEAPONS[v.weapon].reload;
    float s = sinf(p * PI_F);
    gy += (int)(s * 30);
    tilt = (int)(s * 12);
  }
  if (w->mode == MD_DEAD) gy += (int)(w->modeT * 60);
  bool fl = w->flashT > 0;
  int kick = fl ? 4 : 0;
  switch (v.weapon) {
    case W_PISTOL: {
      int x = gx + 6 + tilt, y = gy - 34 + kick;
      if (fl) muzzle(x, y - 3, 6);
      gfx::fillRect(x - 4, y, 9, 22, rgb(50, 50, 56));
      gfx::fillRect(x - 3, y + 1, 3, 20, rgb(95, 95, 105));
      gfx::fillRect(x - 2, y - 2, 5, 3, rgb(25, 25, 28));
      gfx::fillRect(x - 6, y + 14, 13, 8, rgb(60, 40, 30));
      hand(x - 2, y + 26);
      hand(x + 5, y + 28);
      break;
    }
    case W_SHOTGUN: {
      int x = gx + tilt, y = gy - 38 + kick * 2;
      if (fl) muzzle(x, y - 4, 10);
      gfx::fillTriangle(x - 7, y, x - 1, y, x - 12, y + 40, rgb(40, 40, 45));
      gfx::fillTriangle(x - 1, y, x - 12, y + 40, x + 4, y + 40, rgb(40, 40, 45));
      gfx::fillTriangle(x + 1, y, x + 7, y, x + 12, y + 40, rgb(55, 55, 62));
      gfx::fillTriangle(x + 1, y, x + 12, y + 40, x - 4, y + 40, rgb(55, 55, 62));
      gfx::fillRect(x - 6, y - 1, 5, 3, C_BLACK);
      gfx::fillRect(x + 1, y - 1, 5, 3, C_BLACK);
      gfx::vline(x, y, 40, rgb(110, 110, 120));
      int pump = w->reloading ? (int)(sinf(w->reloadT * 12) * 4) : 0;
      gfx::fillRect(x - 10, y + 18 + pump, 21, 9, rgb(110, 60, 30));
      gfx::fillRect(x - 10, y + 18 + pump, 21, 2, rgb(150, 90, 50));
      hand(x - 12, y + 26 + pump);
      hand(x + 13, y + 36);
      break;
    }
    case W_MG: {
      int x = gx + tilt, y = gy - 36 + kick;
      if (fl) muzzle(x + irand(-1, 1), y - 4, 8);
      gfx::fillRect(x - 9, y + 12, 19, 30, rgb(45, 45, 50));
      int spin = (int)(w->levelTime * (input::down(B_FIRE) ? 40 : 4));
      for (int b = 0; b < 3; b++) {
        int bx = x - 6 + b * 5;
        gfx::fillRect(bx, y, 3, 14, rgb(70, 70, 78));
        if (((spin + b) % 3) == 0) gfx::vline(bx + 1, y, 14, rgb(160, 160, 170));
        gfx::pixel(bx + 1, y, C_BLACK);
      }
      gfx::fillRect(x - 11, y + 10, 23, 4, rgb(30, 30, 34));
      gfx::fillRect(x + 8, y + 20, 8, 14, rgb(150, 120, 30));   // mermi kutusu
      for (int i = 0; i < 4; i++) gfx::hline(x + 9, y + 22 + i * 3, 6, rgb(220, 180, 60));
      hand(x - 11, y + 32);
      hand(x + 11, y + 38);
      break;
    }
  }
}

// ---------- HUD ve efektler ----------
static void drawHUD() {
  Inventory& v = w->inv;
  int y = VIEW_H;
  gfx::fillRect(0, y, SCR_W, HUD_H, rgb(22, 6, 6));
  gfx::hline(0, y, SCR_W, rgb(110, 10, 10));
  // can
  uint16_t hc = v.hp > 50 ? C_WHITE : v.hp > 25 ? C_YELLOW : C_RED;
  gfx::fillRect(3, y + 4, 5, 1, C_RED);
  gfx::fillRect(5, y + 2, 1, 5, C_RED);
  gfx::fillRect(4, y + 3, 3, 3, C_RED);
  gfx::textf(10, y + 2, hc, 1, AL_LEFT, "%d", v.hp);
  // zırh
  gfx::fillRect(3, y + 11, 5, 4, rgb(40, 170, 60));
  gfx::pixel(3, y + 14, rgb(22, 6, 6));
  gfx::pixel(7, y + 14, rgb(22, 6, 6));
  gfx::textf(10, y + 10, rgb(120, 220, 120), 1, AL_LEFT, "%d", v.armor);
  // anahtarlar
  for (int k = 1; k <= 3; k++) {
    int kx = 33, ky = y + 1 + (k - 1) * 6;
    if (w->keys & (1 << k)) gfx::fillRect(kx, ky, 6, 4, KEY_COLORS[k]);
    else gfx::rect(kx, ky, 6, 4, rgb(60, 30, 30));
  }
  // öldürme
  gfx::textf(43, y + 2, rgb(170, 140, 140), 1, AL_LEFT, "%d/%d", w->kills, w->totalKills);
  // silah ve mermi
  const WInfo& wi = WEAPONS[v.weapon];
  gfx::text(SCR_W - 2, y + 2, wi.name, rgb(170, 150, 150), 1, AL_RIGHT);
  if (w->reloading) {
    if (fmodf(w->levelTime, 0.4f) < 0.25f) gfx::text(SCR_W - 2, y + 10, "DOLDUR", C_YELLOW, 1, AL_RIGHT);
  } else if (v.mag[v.weapon] == 0 && v.res[v.weapon] == 0) {
    gfx::text(SCR_W - 2, y + 10, "BOS", C_RED, 1, AL_RIGHT);
  } else {
    gfx::textf(SCR_W - 2, y + 10, v.mag[v.weapon] == 0 ? C_RED : C_YELLOW, 1, AL_RIGHT,
               "%d/%d", v.mag[v.weapon], v.res[v.weapon]);
  }
  // süre
  int sec = (int)w->levelTime;
  gfx::textf(43, y + 10, rgb(110, 90, 90), 1, AL_LEFT, "%d:%02d", sec / 60, sec % 60);
}

static void drawEffects() {
  if (w->hurtT > 0) {
    int a = (int)(w->hurtT * 260);
    gfx::blend(0, 0, SCR_W, VIEW_H, rgb(170, 0, 0), a > 110 ? 110 : a);
  }
  if (w->inv.hp < 30 && w->mode == MD_PLAY) {
    int p = (int)((sinf(w->levelTime * 6) * 0.5f + 0.5f) * 140) + 60;
    uint16_t c = rgb(p, 0, 0);
    for (int i = 0; i < 3; i++) gfx::rect(i, i, SCR_W - 2 * i, VIEW_H - 2 * i, shade(c, 256 - i * 70));
  }
  for (const Drop& d : w->drops) {
    if (d.life <= 0.15f) continue;
    int c = d.life > 1 ? 200 : 90 + (int)(d.life * 110);
    gfx::fillCircle((int)d.x, (int)d.y, d.r, rgb(c, 0, 0));
    gfx::vline((int)d.x, (int)d.y - d.r - 4, 4, rgb(c * 3 / 4, 0, 0));
  }
  // nişangah
  int cx = SCR_W / 2, cy = w->horizon;
  gfx::pixel(cx, cy, rgb(255, 60, 40));
  gfx::hline(cx - 4, cy, 2, rgb(160, 30, 20));
  gfx::hline(cx + 3, cy, 2, rgb(160, 30, 20));
  gfx::vline(cx, cy - 4, 2, rgb(160, 30, 20));
  gfx::vline(cx, cy + 3, 2, rgb(160, 30, 20));
  if (w->msgT > 0) gfx::textShadow(SCR_W / 2, 3, w->msg, w->msgColor, 1, AL_CENTER);
}

static void selectRow(int i, int x, int y, int wd, int h, bool sel, float t) {
  (void)t;
  bool locked = i >= w->unlocked;
  if (sel) gfx::fillRect(x, y, wd - 4, h - 1, rgb(70, 6, 6));
  char b[40];
  snprintf(b, sizeof(b), "E1M%d %.12s", i + 1, w->levels.title[i]);
  gfx::text(x + 3, y + 2, b, locked ? rgb(90, 70, 70) : (sel ? C_WHITE : rgb(200, 170, 170)));
  if (locked) {
    gfx::fillRect(x + wd - 13, y + 5, 7, 5, rgb(120, 100, 60));
    gfx::rect(x + wd - 12, y + 2, 5, 4, rgb(120, 100, 60));
  } else if (!w->levels.builtin[i]) {
    gfx::text(x + wd - 16, y + 2, "SD", rgb(80, 160, 80));
  }
}

static void renderSelect() {
  for (int y = 0; y < SCR_H; y++) {
    int v = 18 + (int)(sinf(y * 0.15f + w->modeT * 1.5f) * 6);
    gfx::hline(0, y, SCR_W, rgb(v, 0, 0));
  }
  gfx::textShadow(SCR_W / 2, 4, "DOOM", rgb(230, 30, 20), 2, AL_CENTER);
  gfx::text(SCR_W / 2, 22, "BOLUM SEC", C_GREY, 1, AL_CENTER);
  ui::listDraw(w->list, w->levels.count, 2, 34, SCR_W - 2, 13, 6, selectRow, w->modeT);
  gfx::fillRect(0, 112, SCR_W, 16, C_BLACK);
  int sdn = w->levels.count - w->levels.builtinCount;
  gfx::textf(SCR_W / 2, 113, rgb(120, 100, 100), 1, AL_CENTER, sd::ok() ? "SD: %d ek bolum" : "SD yok: %d gomulu",
             sd::ok() ? sdn : w->levels.builtinCount);
  if (w->list.sel >= 0 && w->list.sel < w->levels.count) {
    char key[16], tb[16];
    save::keyFor(key, "dmt", w->levels.title[w->list.sel]);
    int32_t best = save::get(key, -1);
    if (best >= 0) {
      ui::formatTime(tb, sizeof(tb), (uint32_t)best);
      gfx::textf(SCR_W / 2, 121, rgb(255, 170, 60), 1, AL_CENTER, "En iyi %s", tb);
    } else {
      gfx::text(SCR_W / 2, 121, "ATES: basla", rgb(255, 170, 60), 1, AL_CENTER);
    }
  }
  if (w->msgT > 0) gfx::textShadow(SCR_W / 2, 100, w->msg, w->msgColor, 1, AL_CENTER);
}

static void renderStats() {
  gfx::darken(0, 0, SCR_W, SCR_H, 60);
  ui::panel(6, 14, SCR_W - 12, 100);
  gfx::textShadow(SCR_W / 2, 20, "BOLUM TAMAM", rgb(255, 60, 40), 1, AL_CENTER);
  gfx::text(SCR_W / 2, 31, w->name, rgb(200, 180, 170), 1, AL_CENTER);
  float p = w->modeT / 1.2f;
  if (p > 1) p = 1;
  int kp = w->totalKills ? w->kills * 100 / w->totalKills : 100;
  int sp = w->totalSecrets ? w->secrets * 100 / w->totalSecrets : 100;
  char tb[16], pb[16];
  ui::formatTime(tb, sizeof(tb), (uint32_t)(w->levelTime * 1000 * p));
  ui::formatTime(pb, sizeof(pb), (uint32_t)w->par * 1000);
  gfx::text(14, 46, "Olduruler", C_WHITE);
  gfx::textf(SCR_W - 14, 46, C_YELLOW, 1, AL_RIGHT, "%d%%", (int)(kp * p));
  gfx::text(14, 58, "Gizli oda", C_WHITE);
  gfx::textf(SCR_W - 14, 58, C_YELLOW, 1, AL_RIGHT, "%d%%", (int)(sp * p));
  gfx::text(14, 70, "Sure", C_WHITE);
  gfx::text(SCR_W - 14, 70, tb, C_YELLOW, 1, AL_RIGHT);
  gfx::text(14, 82, "Hedef", C_GREY);
  gfx::text(SCR_W - 14, 82, pb, C_GREY, 1, AL_RIGHT);
  if (w->modeT > 0.8f && fmodf(w->modeT, 1.0f) < 0.7f)
    gfx::text(SCR_W / 2, 100, w->levelIdx + 1 < w->levels.count ? "ATES: sonraki" : "ATES: devam",
              rgb(255, 120, 80), 1, AL_CENTER);
}

static void render() {
  if (w->mode == MD_SELECT) { renderSelect(); return; }
  if (w->mode == MD_VICTORY) {
    gfx::vgradient(0, 0, SCR_W, SCR_H, rgb(60, 0, 0), C_BLACK);
    gfx::textShadow(SCR_W / 2, 24, "CEHENNEM", rgb(255, 50, 30), 2, AL_CENTER);
    gfx::textShadow(SCR_W / 2, 44, "SUSTU", rgb(255, 50, 30), 2, AL_CENTER);
    gfx::text(SCR_W / 2, 72, "Tum bolumler bitti.", C_WHITE, 1, AL_CENTER);
    gfx::text(SCR_W / 2, 84, "Simdilik...", rgb(160, 120, 120), 1, AL_CENTER);
    if (w->modeT > 1.0f) gfx::text(SCR_W / 2, 110, "ATES: bolum secimi", C_GREY, 1, AL_CENTER);
    return;
  }
  renderWorld();
  renderSprites();
  if (w->mode == MD_STATS) { renderStats(); return; }
  drawWeapon();
  drawEffects();
  drawHUD();
  if (w->mode == MD_DEAD) {
    int a = (int)(w->modeT * 120);
    gfx::blend(0, 0, SCR_W, VIEW_H, rgb(90, 0, 0), a > 170 ? 170 : a);
    if (w->modeT > 0.6f) ui::banner("OLDUN", w->modeT > 1.2f ? "ATES: tekrar" : nullptr, rgb(255, 40, 30));
  }
}

// ---------- Game arayüzü ----------
static bool enter() {
  void* mem = calloc(1, sizeof(World));
  if (!mem) return false;
  w = new (mem) World;
  for (int c = 0; c < 128; c++) w->pal[c] = palColor((char)c);
  genTextures(w->tex);
  w->levels.build(BUILTIN_LEVELS, BUILTIN_COUNT, "/doom", ".lvl");
  w->unlocked = save::get("dm_unl", 1);
  if (w->unlocked > w->levels.count) w->unlocked = w->levels.count;
  if (w->unlocked < 1) w->unlocked = 1;
  w->list.sel = w->unlocked - 1;
  w->mode = MD_SELECT;
  freshInventory();
  hwLog("Doom: World %u B, %d bolum", (unsigned)sizeof(World), w->levels.count);
  return true;
}

static void exitGame() {
  if (w) { w->~World(); free(w); }
  w = nullptr;
}

static void icon(int x, int y, float t) {
  const Art& a = art(A_DEMON);
  gfx::art(x, y, a.rows, a.w, a.h, 1, palColor, t > 0 && fmodf(t, 1.0f) < 0.5f);
}

static void scoreText(char* buf, int n) {
  int total = BUILTIN_COUNT + sd::count("/doom", ".lvl");
  int u = save::get("dm_unl", 1);
  if (u > total) u = total;
  snprintf(buf, n, "Bolum %d/%d acik", u, total);
}

static const char* const CONTROLS[] = {
  "ILERI",  "Yuru/kapi ac",
  "GERI",   "Geri yuru",
  "SOL/SAG", "Don",
  "ATES",   "Ates et",
  "RELOAD", "Sarjor degis",
  "RELx2",  "Sonraki silah",
  nullptr, nullptr,
};

}  // namespace doom

extern const Game DOOM_GAME = {
  "DOOM", doom::icon, doom::scoreText, doom::CONTROLS,
  doom::enter, doom::update, doom::render, doom::exitGame,
};
