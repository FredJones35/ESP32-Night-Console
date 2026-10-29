// Doom tarzı raycaster (yeni yapıya taşınmış ilk sürüm)
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../core/game.h"
#include "../../core/gfx.h"
#include "../../core/hw.h"
#include "../../core/input.h"
#include "../../core/save.h"

namespace doom {

static const int HUD_H  = 18;
static const int VIEW_H = SCR_H - HUD_H;
static const int TEX    = 32;
static const int MAP_W  = 16;
static const int MAP_H  = 16;
static const int NUM_EN = 8;

static const char* const LEVEL[MAP_H] = {
  "1111111111111111", "1..............1", "1..222....333..1", "1..2........3..1",
  "1..2..1111..3..1", "1.....1..1.....1", "1.....1..1.....1", "1..3........2..1",
  "1..3..2222..2..1", "1..333....222..1", "1..............1", "1.1111..1111...1",
  "1.1........1...1", "1.1..3333..1...1", "1..............1", "1111111111111111",
};

static const char* const IMP[16] = {
  "......XXXX......", ".....XXXXXX.....", "....XXYXXYXX....", "....XXXXXXXX....",
  ".....XRRRRX.....", "......XXXX......", "...XXXXXXXXXX...", "..XXXXXXXXXXXX..",
  "..XX.XXXXXX.XX..", "..XX.XXXXXX.XX..", "..BB.XXXXXX.BB..", ".....XXXXXX.....",
  ".....XX..XX.....", ".....XX..XX.....", ".....XX..XX.....", "....BBB..BBB....",
};

static const float EN_START[NUM_EN][2] = {
  {7.5f, 5.5f}, {13.5f, 5.5f}, {5.5f, 10.5f}, {10.5f, 12.5f},
  {13.5f, 13.5f}, {8.5f, 2.5f}, {3.5f, 14.5f}, {1.5f, 8.5f},
};

enum State { PLAYING, DEAD, WIN };

struct Enemy {
  float x, y;
  int8_t hp;
  bool alive, aware;
  float atkCd, hurt, deadT;
  int scrX, scrW;
  float depth;
};

struct World {
  uint16_t tex[3][TEX * TEX];
  uint16_t ceilCol[VIEW_H], floorCol[VIEW_H];
  float zbuf[SCR_W];
  float posX, posY, ang, dirX, dirY, planeX, planeY;
  int hp, ammo, kills;
  float fireCd, flashT, hurtT, walkT;
  bool walking;
  State state;
  Enemy en[NUM_EN];
};
static World* w = nullptr;

static inline uint8_t cell(int x, int y) {
  if (x < 0 || y < 0 || x >= MAP_W || y >= MAP_H) return 1;
  char c = LEVEL[y][x];
  return c == '.' ? 0 : (uint8_t)(c - '0');
}

static void updateCamera() {
  w->dirX = cosf(w->ang);  w->dirY = sinf(w->ang);
  w->planeX = -w->dirY * 0.66f;  w->planeY = w->dirX * 0.66f;
}

static void tryMove(float& x, float& y, float mx, float my, float r) {
  float nx = x + mx;
  if (!cell((int)(nx + (mx > 0 ? r : -r)), (int)y)) x = nx;
  float ny = y + my;
  if (!cell((int)x, (int)(ny + (my > 0 ? r : -r)))) y = ny;
}

static bool lineOfSight(float x0, float y0, float x1, float y1) {
  float dx = x1 - x0, dy = y1 - y0;
  float d = sqrtf(dx * dx + dy * dy);
  int steps = (int)(d / 0.1f) + 1;
  for (int i = 1; i < steps; i++) {
    float t = (float)i / steps;
    if (cell((int)(x0 + dx * t), (int)(y0 + dy * t))) return false;
  }
  return true;
}

static void genTextures() {
  for (int y = 0; y < TEX; y++) {
    for (int x = 0; x < TEX; x++) {
      uint32_t n = hash32(x * 73856093u ^ y * 19349663u);
      int nz = (int)(n & 31) - 16;
      int i = y * TEX + x;
      int off = ((y / 8) & 1) * 8;
      bool mortar = (y % 8 == 0) || ((x + off) % 16 == 0);
      w->tex[0][i] = mortar ? rgb(90 + nz / 2, 85 + nz / 2, 80 + nz / 2)
                            : rgb(150 + nz, 55 + nz / 2, 35 + nz / 2);
      bool edge  = (x % 16 == 0) || (y % 16 == 0);
      bool rivet = ((x % 16 == 3) || (x % 16 == 12)) && ((y % 16 == 3) || (y % 16 == 12));
      uint16_t c;
      if (edge)                     c = rgb(40, 40, 48);
      else if (rivet)               c = rgb(200, 200, 210);
      else if (y == 22 || y == 23)  c = rgb(0, 180, 200);
      else                          c = rgb(110 + nz, 110 + nz, 122 + nz);
      w->tex[1][i] = c;
      uint32_t b = hash32((x / 8) + (y / 8) * 13 + 999);
      int base = 70 + (int)(b % 50);
      bool e2 = (x % 8 == 0) || (y % 8 == 0);
      w->tex[2][i] = e2 ? rgb(20, 40, 20) : rgb(base / 3 + nz / 2, base + nz, base / 3 + nz / 2);
    }
  }
  for (int y = 0; y < VIEW_H; y++) {
    if (y < VIEW_H / 2) w->ceilCol[y] = sw16(shade(rgb(80, 80, 95), 200 - y * 3));
    else w->floorCol[y] = sw16(shade(rgb(110, 80, 55), 50 + (y - VIEW_H / 2) * 4));
  }
}

static void resetGame() {
  w->posX = 1.5f; w->posY = 1.5f; w->ang = 0.0f;
  updateCamera();
  w->hp = 100; w->ammo = 40; w->kills = 0;
  w->fireCd = 0.3f; w->flashT = w->hurtT = w->walkT = 0;
  w->state = PLAYING;
  for (int i = 0; i < NUM_EN; i++) {
    Enemy& e = w->en[i];
    e.x = EN_START[i][0]; e.y = EN_START[i][1];
    e.hp = 3; e.alive = true; e.aware = false;
    e.atkCd = 0.5f; e.hurt = 0; e.deadT = 0;
    e.scrW = 0; e.depth = 1e9f;
  }
}

static void shoot() {
  int best = -1; float bd = 1e9f;
  for (int i = 0; i < NUM_EN; i++) {
    Enemy& e = w->en[i];
    if (!e.alive || e.scrW <= 0) continue;
    if (e.depth >= w->zbuf[SCR_W / 2]) continue;
    int tol = e.scrW / 3; if (tol < 2) tol = 2;
    if (abs(e.scrX - SCR_W / 2) <= tol && e.depth < bd) { bd = e.depth; best = i; }
  }
  if (best < 0) return;
  Enemy& e = w->en[best];
  e.hp -= 1 + (int)(hwRandom() % 2);
  e.hurt = 0.15f;
  e.aware = true;
  if (e.hp <= 0) { e.alive = false; e.deadT = 0.4f; w->kills++; w->ammo += 4; }
}

static void update(float dt) {
  if (w->state != PLAYING) {
    if (input::pressed(B_FIRE)) { resetGame(); input::clear(); }
    return;
  }
  const float MOVE = 2.6f, ROT = 2.6f;
  if (input::down(B_LEFT))  w->ang -= ROT * dt;
  if (input::down(B_RIGHT)) w->ang += ROT * dt;
  updateCamera();
  float mv = 0;
  if (input::down(B_FWD))  mv += MOVE * dt;
  if (input::down(B_BACK)) mv -= MOVE * dt * 0.8f;
  w->walking = (mv != 0);
  if (w->walking) { w->walkT += dt; tryMove(w->posX, w->posY, w->dirX * mv, w->dirY * mv, 0.2f); }

  w->fireCd -= dt; w->flashT -= dt; w->hurtT -= dt;
  if (input::down(B_FIRE) && w->fireCd <= 0 && w->ammo > 0) {
    shoot(); w->fireCd = 0.35f; w->flashT = 0.08f; w->ammo--;
  }
  for (int i = 0; i < NUM_EN; i++) {
    Enemy& e = w->en[i];
    if (!e.alive) { if (e.deadT > 0) e.deadT -= dt; continue; }
    if (e.hurt > 0) e.hurt -= dt;
    float dx = w->posX - e.x, dy = w->posY - e.y;
    float d = sqrtf(dx * dx + dy * dy);
    if (!e.aware && d < 9.0f && lineOfSight(e.x, e.y, w->posX, w->posY)) e.aware = true;
    if (!e.aware) continue;
    if (d > 0.85f) {
      float s = 1.3f * dt;
      tryMove(e.x, e.y, dx / d * s, dy / d * s, 0.25f);
      e.atkCd = 0.4f;
    } else {
      e.atkCd -= dt;
      if (e.atkCd <= 0) { w->hp -= 6 + (int)(hwRandom() % 8); w->hurtT = 0.25f; e.atkCd = 1.0f; }
    }
  }
  if (w->hp <= 0) { w->hp = 0; w->state = DEAD; }
  else if (w->kills == NUM_EN) w->state = WIN;
}

static void renderWorld() {
  for (int x = 0; x < SCR_W; x++) {
    float camX = 2.0f * x / SCR_W - 1.0f;
    float rdx = w->dirX + w->planeX * camX, rdy = w->dirY + w->planeY * camX;
    int mx = (int)w->posX, my = (int)w->posY;
    float ddx = (rdx == 0) ? 1e30f : fabsf(1.0f / rdx);
    float ddy = (rdy == 0) ? 1e30f : fabsf(1.0f / rdy);
    int stepX, stepY; float sdx, sdy;
    if (rdx < 0) { stepX = -1; sdx = (w->posX - mx) * ddx; } else { stepX = 1; sdx = (mx + 1.0f - w->posX) * ddx; }
    if (rdy < 0) { stepY = -1; sdy = (w->posY - my) * ddy; } else { stepY = 1; sdy = (my + 1.0f - w->posY) * ddy; }
    int side = 0; uint8_t hit = 0;
    for (int i = 0; i < 64 && !hit; i++) {
      if (sdx < sdy) { sdx += ddx; mx += stepX; side = 0; }
      else           { sdy += ddy; my += stepY; side = 1; }
      hit = cell(mx, my);
    }
    if (!hit) hit = 1;
    float perp = side == 0 ? (sdx - ddx) : (sdy - ddy);
    if (perp < 0.05f) perp = 0.05f;
    w->zbuf[x] = perp;
    float wallX = side == 0 ? w->posY + perp * rdy : w->posX + perp * rdx;
    wallX -= floorf(wallX);
    int texX = (int)(wallX * TEX) & (TEX - 1);
    if (side == 0 && rdx > 0) texX = TEX - 1 - texX;
    if (side == 1 && rdy < 0) texX = TEX - 1 - texX;
    int lineH = (int)(VIEW_H / perp); if (lineH < 1) lineH = 1;
    int top = VIEW_H / 2 - lineH / 2;
    int y0 = top < 0 ? 0 : top;
    int y1 = top + lineH; if (y1 > VIEW_H) y1 = VIEW_H;
    int f = (int)(256.0f / (1.0f + perp * 0.22f));
    if (side) f = f * 3 / 4;
    const uint16_t* t = w->tex[hit - 1];
    uint16_t* col = fb + x;
    for (int y = 0; y < y0; y++) col[y * SCR_W] = w->ceilCol[y];
    uint32_t step = ((uint32_t)TEX << 16) / (uint32_t)lineH;
    uint32_t tp = (uint32_t)(y0 - top) * step;
    for (int y = y0; y < y1; y++) {
      int ty = (tp >> 16) & (TEX - 1);
      tp += step;
      col[y * SCR_W] = sw16(shade(t[ty * TEX + texX], f));
    }
    for (int y = y1; y < VIEW_H; y++) col[y * SCR_W] = w->floorCol[y];
  }
}

static uint16_t impColor(char p) {
  switch (p) {
    case 'Y': return rgb(255, 230, 0);
    case 'R': return rgb(200, 0, 0);
    case 'B': return rgb(60, 30, 20);
    default:  return rgb(160, 90, 50);
  }
}

static void renderSprites() {
  float invDet = 1.0f / (w->planeX * w->dirY - w->dirX * w->planeY);
  int order[NUM_EN]; float dist[NUM_EN];
  for (int i = 0; i < NUM_EN; i++) {
    order[i] = i;
    float dx = w->en[i].x - w->posX, dy = w->en[i].y - w->posY;
    dist[i] = dx * dx + dy * dy;
    w->en[i].scrW = 0;
  }
  for (int i = 1; i < NUM_EN; i++) {
    int k = order[i], j = i - 1;
    while (j >= 0 && dist[order[j]] < dist[k]) { order[j + 1] = order[j]; j--; }
    order[j + 1] = k;
  }
  for (int n = 0; n < NUM_EN; n++) {
    Enemy& e = w->en[order[n]];
    if (!e.alive && e.deadT <= 0) continue;
    float sx = e.x - w->posX, sy = e.y - w->posY;
    float tx = invDet * (w->dirY * sx - w->dirX * sy);
    float ty = invDet * (-w->planeY * sx + w->planeX * sy);
    if (ty <= 0.1f) continue;
    int scrX = (int)((SCR_W / 2) * (1.0f + tx / ty));
    int h  = (int)(VIEW_H / ty);
    int sh = h * 3 / 4; if (sh < 2) sh = 2;
    int top = VIEW_H / 2 + h / 2 - sh;
    e.scrX = scrX; e.scrW = sh; e.depth = ty;
    int f = (int)(256.0f / (1.0f + ty * 0.2f));
    int x0 = scrX - sh / 2;
    for (int stripe = x0; stripe < x0 + sh; stripe++) {
      if (stripe < 0 || stripe >= SCR_W) continue;
      if (ty >= w->zbuf[stripe]) continue;
      int u = (stripe - x0) * 16 / sh;
      int ya = top < 0 ? 0 : top;
      int yb = top + sh; if (yb > VIEW_H) yb = VIEW_H;
      for (int y = ya; y < yb; y++) {
        int v = (y - top) * 16 / sh;
        char p = IMP[v][u];
        if (p == '.') continue;
        uint16_t c;
        if (!e.alive)        c = rgb(180, 0, 0);
        else if (e.hurt > 0) c = C_WHITE;
        else                 c = shade(impColor(p), f);
        fb[y * SCR_W + stripe] = sw16(c);
      }
    }
  }
}

static void drawGun() {
  int bob  = w->walking ? (int)(sinf(w->walkT * 9.0f) * 2.5f) : 0;
  int sway = w->walking ? (int)(cosf(w->walkT * 4.5f) * 2.0f) : 0;
  int kick = w->flashT > 0 ? 3 : 0;
  int gx = SCR_W / 2 + sway;
  int gy = VIEW_H - 24 + abs(bob) + kick;
  if (w->flashT > 0) {
    gfx::fillCircle(gx, gy - 6, 8, C_ORANGE);
    gfx::fillCircle(gx, gy - 6, 5, C_YELLOW);
    gfx::fillCircle(gx, gy - 6, 2, C_WHITE);
  }
  gfx::fillRect(gx - 4, gy - 2, 8, 26, rgb(70, 70, 75));
  gfx::fillRect(gx - 2, gy - 4, 4, 4, rgb(35, 35, 35));
  gfx::vline(gx - 1, gy, 22, rgb(140, 140, 150));
  gfx::fillRect(gx - 6, gy + 8, 12, 6, rgb(110, 60, 30));
  gfx::fillCircle(gx - 8, gy + 19, 5, rgb(230, 170, 120));
  gfx::fillCircle(gx + 8, gy + 20, 5, rgb(230, 170, 120));
}

static void drawHUD() {
  gfx::fillRect(0, VIEW_H, SCR_W, HUD_H, rgb(45, 45, 45));
  gfx::hline(0, VIEW_H, SCR_W, rgb(120, 120, 120));
  int ty = VIEW_H + 6;
  gfx::text(3, ty, "HP", C_RED);      gfx::textf(17, ty, C_WHITE, 1, AL_LEFT, "%d", w->hp);
  gfx::text(44, ty, "AM", C_YELLOW);  gfx::textf(58, ty, C_WHITE, 1, AL_LEFT, "%d", w->ammo);
  gfx::text(88, ty, "K", C_GREEN);    gfx::textf(96, ty, C_WHITE, 1, AL_LEFT, "%d/%d", w->kills, NUM_EN);
}

static void render() {
  renderWorld();
  renderSprites();
  drawGun();
  gfx::hline(SCR_W / 2 - 3, VIEW_H / 2, 2, C_GREEN);
  gfx::hline(SCR_W / 2 + 2, VIEW_H / 2, 2, C_GREEN);
  gfx::vline(SCR_W / 2, VIEW_H / 2 - 3, 2, C_GREEN);
  gfx::vline(SCR_W / 2, VIEW_H / 2 + 2, 2, C_GREEN);
  if (w->hurtT > 0 || w->state == DEAD)
    for (int i = 0; i < 3; i++) gfx::rect(i, i, SCR_W - 2 * i, VIEW_H - 2 * i, C_RED);
  drawHUD();
  if (w->state == DEAD) {
    gfx::text(SCR_W / 2, 34, "OLDUN", C_RED, 2, AL_CENTER);
    gfx::text(SCR_W / 2, 60, "Ates = Tekrar", C_WHITE, 1, AL_CENTER);
  } else if (w->state == WIN) {
    gfx::text(SCR_W / 2, 30, "BOLUM", C_GREEN, 2, AL_CENTER);
    gfx::text(SCR_W / 2, 48, "TAMAM", C_GREEN, 2, AL_CENTER);
    gfx::text(SCR_W / 2, 70, "Ates = Tekrar", C_WHITE, 1, AL_CENTER);
  }
}

static bool enter() {
  w = (World*)calloc(1, sizeof(World));
  if (!w) return false;
  genTextures();
  resetGame();
  return true;
}

static void exitGame() {
  free(w);
  w = nullptr;
}

static void icon(int x, int y, float t) {
  (void)t;
  gfx::art(x + 4, y + 4, IMP, 16, 16, 1, impColor);
}

static void scoreText(char* buf, int n) { snprintf(buf, n, "Raycaster"); }

static const char* const CONTROLS[] = {
  "ILERI", "Yuru",
  "GERI", "Geri yuru",
  "SOLA", "Sola don",
  "SAGA", "Saga don",
  "ATES", "Ates et",
  nullptr, nullptr,
};

}  // namespace doom

extern const Game DOOM_GAME = {
  "DOOM", doom::icon, doom::scoreText, doom::CONTROLS,
  doom::enter, doom::update, doom::render, doom::exitGame,
};
