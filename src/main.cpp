// ESP32 DOOM-tarzı raycaster
// Donanım: ESP32 DevKit V1, 1.44" 128x128 ST7735 SPI TFT, 5 buton (GND'ye)
//
// Ekran:  SDA->D13  SCK->D14  CS->D15  A0/DC->D27  RST->D26  VCC->3V3  GND->GND  LED/BL->3V3
// Buton:  ATES->D19  ILERI->D4  SOLA->D32  SAGA->D25  GERI->D33

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <math.h>

#define BTN_FIRE  19
#define BTN_FWD    4
#define BTN_LEFT  32
#define BTN_RIGHT 25
#define BTN_BACK  33

static const int SCR_W  = 128;
static const int SCR_H  = 128;
static const int HUD_H  = 18;
static const int VIEW_H = SCR_H - HUD_H;
static const int TEX    = 32;
static const int MAP_W  = 16;
static const int MAP_H  = 16;

static const char* LEVEL[MAP_H] = {
  "1111111111111111",
  "1..............1",
  "1..222....333..1",
  "1..2........3..1",
  "1..2..1111..3..1",
  "1.....1..1.....1",
  "1.....1..1.....1",
  "1..3........2..1",
  "1..3..2222..2..1",
  "1..333....222..1",
  "1..............1",
  "1.1111..1111...1",
  "1.1........1...1",
  "1.1..3333..1...1",
  "1..............1",
  "1111111111111111"
};

// 16x16 imp: X gövde, Y göz, R ağız, B pençe, . saydam
static const char* IMP[16] = {
  "......XXXX......",
  ".....XXXXXX.....",
  "....XXYXXYXX....",
  "....XXXXXXXX....",
  ".....XRRRRX.....",
  "......XXXX......",
  "...XXXXXXXXXX...",
  "..XXXXXXXXXXXX..",
  "..XX.XXXXXX.XX..",
  "..XX.XXXXXX.XX..",
  "..BB.XXXXXX.BB..",
  ".....XXXXXX.....",
  ".....XX..XX.....",
  ".....XX..XX.....",
  ".....XX..XX.....",
  "....BBB..BBB...."
};

TFT_eSPI tft;
TFT_eSprite spr = TFT_eSprite(&tft);
static uint16_t* fb = nullptr;   // sprite framebuffer (byte-swapped RGB565)

static uint16_t tex[3][TEX * TEX];
static uint16_t ceilCol[VIEW_H], floorCol[VIEW_H];
static float zbuf[SCR_W];

// ---------- oyuncu ----------
static float posX, posY, ang, dirX, dirY, planeX, planeY;
static int   hp, ammo, kills;
static float fireCd, flashT, hurtT, walkT;
static bool  walking, prevFire;

enum State { TITLE, PLAYING, DEAD, WIN };
static State state = TITLE;

// ---------- düşmanlar ----------
struct Enemy {
  float x, y;
  int8_t hp;
  bool alive, aware;
  float atkCd, hurt, deadT;
  int scrX, scrW;
  float depth;
};
static const int NUM_EN = 8;
static Enemy en[NUM_EN];
static const float EN_START[NUM_EN][2] = {
  {7.5f, 5.5f}, {13.5f, 5.5f}, {5.5f, 10.5f}, {10.5f, 12.5f},
  {13.5f, 13.5f}, {8.5f, 2.5f}, {3.5f, 14.5f}, {1.5f, 8.5f}
};

// ---------- yardımcılar ----------
static inline uint16_t sw(uint16_t c) { return (c >> 8) | (c << 8); }
static inline uint8_t  cl(int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); }
static inline uint16_t col565(int r, int g, int b) {
  return ((cl(r) & 0xF8) << 8) | ((cl(g) & 0xFC) << 3) | (cl(b) >> 3);
}
static inline uint16_t shade(uint16_t c, int f) {  // f: 0..256
  if (f >= 256) return c;
  if (f < 0) f = 0;
  uint16_t r = (((c >> 11) & 31) * f) >> 8;
  uint16_t g = (((c >> 5) & 63) * f) >> 8;
  uint16_t b = ((c & 31) * f) >> 8;
  return (r << 11) | (g << 5) | b;
}
static inline uint32_t hash32(uint32_t x) {
  x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
  return x;
}
static inline uint8_t cell(int x, int y) {
  if (x < 0 || y < 0 || x >= MAP_W || y >= MAP_H) return 1;
  char c = LEVEL[y][x];
  return c == '.' ? 0 : (uint8_t)(c - '0');
}
static inline bool pressed(int pin) { return digitalRead(pin) == LOW; }

static void updateCamera() {
  dirX = cosf(ang);  dirY = sinf(ang);
  planeX = -dirY * 0.66f;  planeY = dirX * 0.66f;
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

// ---------- prosedürel dokular ----------
static void genTextures() {
  for (int y = 0; y < TEX; y++) {
    for (int x = 0; x < TEX; x++) {
      uint32_t n = hash32(x * 73856093u ^ y * 19349663u);
      int nz = (int)(n & 31) - 16;
      int i = y * TEX + x;

      // 1: tuğla
      int off = ((y / 8) & 1) * 8;
      bool mortar = (y % 8 == 0) || ((x + off) % 16 == 0);
      tex[0][i] = mortar ? col565(90 + nz / 2, 85 + nz / 2, 80 + nz / 2)
                         : col565(150 + nz, 55 + nz / 2, 35 + nz / 2);

      // 2: metal panel
      bool edge  = (x % 16 == 0) || (y % 16 == 0);
      bool rivet = ((x % 16 == 3) || (x % 16 == 12)) && ((y % 16 == 3) || (y % 16 == 12));
      uint16_t c;
      if (edge)                     c = col565(40, 40, 48);
      else if (rivet)               c = col565(200, 200, 210);
      else if (y == 22 || y == 23)  c = col565(0, 180, 200);
      else                          c = col565(110 + nz, 110 + nz, 122 + nz);
      tex[1][i] = c;

      // 3: yeşil taş
      uint32_t b = hash32((x / 8) + (y / 8) * 13 + 999);
      int base = 70 + (int)(b % 50);
      bool e2 = (x % 8 == 0) || (y % 8 == 0);
      tex[2][i] = e2 ? col565(20, 40, 20)
                     : col565(base / 3 + nz / 2, base + nz, base / 3 + nz / 2);
    }
  }
  for (int y = 0; y < VIEW_H; y++) {
    if (y < VIEW_H / 2) {
      ceilCol[y] = sw(shade(col565(80, 80, 95), 200 - y * 3));
    } else {
      int d = y - VIEW_H / 2;
      floorCol[y] = sw(shade(col565(110, 80, 55), 50 + d * 4));
    }
  }
}

// ---------- oyun durumu ----------
static void resetGame() {
  posX = 1.5f; posY = 1.5f; ang = 0.0f;
  updateCamera();
  hp = 100; ammo = 40; kills = 0;
  fireCd = flashT = hurtT = walkT = 0;
  for (int i = 0; i < NUM_EN; i++) {
    en[i].x = EN_START[i][0]; en[i].y = EN_START[i][1];
    en[i].hp = 3; en[i].alive = true; en[i].aware = false;
    en[i].atkCd = 0.5f; en[i].hurt = 0; en[i].deadT = 0;
    en[i].scrW = 0; en[i].depth = 1e9f;
  }
}

static void shoot() {
  int best = -1; float bd = 1e9f;
  for (int i = 0; i < NUM_EN; i++) {
    Enemy& e = en[i];
    if (!e.alive || e.scrW <= 0) continue;
    if (e.depth >= zbuf[SCR_W / 2]) continue;   // duvarın arkasında
    int tol = e.scrW / 3; if (tol < 2) tol = 2;
    if (abs(e.scrX - SCR_W / 2) <= tol && e.depth < bd) { bd = e.depth; best = i; }
  }
  if (best < 0) return;
  Enemy& e = en[best];
  e.hp -= random(1, 3);
  e.hurt = 0.15f;
  e.aware = true;
  if (e.hp <= 0) { e.alive = false; e.deadT = 0.4f; kills++; ammo += 4; }
}

static void update(float dt) {
  bool f = pressed(BTN_FIRE);
  if (state != PLAYING) {
    if (f && !prevFire) { resetGame(); state = PLAYING; fireCd = 0.3f; }
    prevFire = f;
    return;
  }

  const float MOVE = 2.6f, ROT = 2.6f;
  if (pressed(BTN_LEFT))  ang -= ROT * dt;
  if (pressed(BTN_RIGHT)) ang += ROT * dt;
  updateCamera();

  float mv = 0;
  if (pressed(BTN_FWD))  mv += MOVE * dt;
  if (pressed(BTN_BACK)) mv -= MOVE * dt * 0.8f;
  walking = (mv != 0);
  if (walking) { walkT += dt; tryMove(posX, posY, dirX * mv, dirY * mv, 0.2f); }

  fireCd -= dt; flashT -= dt; hurtT -= dt;
  if (f && fireCd <= 0 && ammo > 0) { shoot(); fireCd = 0.35f; flashT = 0.08f; ammo--; }
  prevFire = f;

  for (int i = 0; i < NUM_EN; i++) {
    Enemy& e = en[i];
    if (!e.alive) { if (e.deadT > 0) e.deadT -= dt; continue; }
    if (e.hurt > 0) e.hurt -= dt;
    float dx = posX - e.x, dy = posY - e.y;
    float d = sqrtf(dx * dx + dy * dy);
    if (!e.aware && d < 9.0f && lineOfSight(e.x, e.y, posX, posY)) e.aware = true;
    if (!e.aware) continue;
    if (d > 0.85f) {
      float s = 1.3f * dt;
      tryMove(e.x, e.y, dx / d * s, dy / d * s, 0.25f);
      e.atkCd = 0.4f;
    } else {
      e.atkCd -= dt;
      if (e.atkCd <= 0) { hp -= random(6, 13); hurtT = 0.25f; e.atkCd = 1.0f; }
    }
  }

  if (hp <= 0) { hp = 0; state = DEAD; }
  else if (kills == NUM_EN) state = WIN;
}

// ---------- çizim ----------
static void renderWorld() {
  for (int x = 0; x < SCR_W; x++) {
    float camX = 2.0f * x / SCR_W - 1.0f;
    float rdx = dirX + planeX * camX, rdy = dirY + planeY * camX;
    int mx = (int)posX, my = (int)posY;
    float ddx = (rdx == 0) ? 1e30f : fabsf(1.0f / rdx);
    float ddy = (rdy == 0) ? 1e30f : fabsf(1.0f / rdy);
    int stepX, stepY; float sdx, sdy;
    if (rdx < 0) { stepX = -1; sdx = (posX - mx) * ddx; } else { stepX = 1; sdx = (mx + 1.0f - posX) * ddx; }
    if (rdy < 0) { stepY = -1; sdy = (posY - my) * ddy; } else { stepY = 1; sdy = (my + 1.0f - posY) * ddy; }

    int side = 0; uint8_t hit = 0;
    for (int i = 0; i < 64 && !hit; i++) {
      if (sdx < sdy) { sdx += ddx; mx += stepX; side = 0; }
      else           { sdy += ddy; my += stepY; side = 1; }
      hit = cell(mx, my);
    }
    if (!hit) hit = 1;

    float perp = side == 0 ? (sdx - ddx) : (sdy - ddy);
    if (perp < 0.05f) perp = 0.05f;
    zbuf[x] = perp;

    float wallX = side == 0 ? posY + perp * rdy : posX + perp * rdx;
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

    const uint16_t* t = tex[hit - 1];
    uint16_t* col = fb + x;
    for (int y = 0; y < y0; y++) col[y * SCR_W] = ceilCol[y];
    uint32_t step = ((uint32_t)TEX << 16) / (uint32_t)lineH;
    uint32_t tp = (uint32_t)(y0 - top) * step;
    for (int y = y0; y < y1; y++) {
      int ty = (tp >> 16) & (TEX - 1);
      tp += step;
      col[y * SCR_W] = sw(shade(t[ty * TEX + texX], f));
    }
    for (int y = y1; y < VIEW_H; y++) col[y * SCR_W] = floorCol[y];
  }
}

static uint16_t impColor(char p) {
  switch (p) {
    case 'Y': return col565(255, 230, 0);
    case 'R': return col565(200, 0, 0);
    case 'B': return col565(60, 30, 20);
    default:  return col565(160, 90, 50);
  }
}

static void renderSprites() {
  float invDet = 1.0f / (planeX * dirY - dirX * planeY);
  int order[NUM_EN]; float dist[NUM_EN];
  for (int i = 0; i < NUM_EN; i++) {
    order[i] = i;
    float dx = en[i].x - posX, dy = en[i].y - posY;
    dist[i] = dx * dx + dy * dy;
    en[i].scrW = 0;
  }
  // uzaktan yakına sırala
  for (int i = 1; i < NUM_EN; i++) {
    int k = order[i], j = i - 1;
    while (j >= 0 && dist[order[j]] < dist[k]) { order[j + 1] = order[j]; j--; }
    order[j + 1] = k;
  }

  for (int n = 0; n < NUM_EN; n++) {
    Enemy& e = en[order[n]];
    if (!e.alive && e.deadT <= 0) continue;
    float sx = e.x - posX, sy = e.y - posY;
    float tx = invDet * (dirY * sx - dirX * sy);
    float ty = invDet * (-planeY * sx + planeX * sy);
    if (ty <= 0.1f) continue;

    int scrX = (int)((SCR_W / 2) * (1.0f + tx / ty));
    int h  = (int)(VIEW_H / ty);
    int sh = h * 3 / 4; if (sh < 2) sh = 2;
    int top = VIEW_H / 2 + h / 2 - sh;     // ayaklar zeminde
    e.scrX = scrX; e.scrW = sh; e.depth = ty;

    int f = (int)(256.0f / (1.0f + ty * 0.2f));
    int x0 = scrX - sh / 2;
    for (int stripe = x0; stripe < x0 + sh; stripe++) {
      if (stripe < 0 || stripe >= SCR_W) continue;
      if (ty >= zbuf[stripe]) continue;
      int u = (stripe - x0) * 16 / sh;
      int ya = top < 0 ? 0 : top;
      int yb = top + sh; if (yb > VIEW_H) yb = VIEW_H;
      for (int y = ya; y < yb; y++) {
        int v = (y - top) * 16 / sh;
        char p = IMP[v][u];
        if (p == '.') continue;
        uint16_t c;
        if (!e.alive)        c = col565(180, 0, 0);
        else if (e.hurt > 0) c = TFT_WHITE;
        else                 c = shade(impColor(p), f);
        fb[y * SCR_W + stripe] = sw(c);
      }
    }
  }
}

static void drawGun() {
  int bob  = walking ? (int)(sinf(walkT * 9.0f) * 2.5f) : 0;
  int sway = walking ? (int)(cosf(walkT * 4.5f) * 2.0f) : 0;
  int kick = flashT > 0 ? 3 : 0;
  int gx = SCR_W / 2 + sway;
  int gy = VIEW_H - 24 + abs(bob) + kick;

  if (flashT > 0) {
    spr.fillCircle(gx, gy - 6, 8, TFT_ORANGE);
    spr.fillCircle(gx, gy - 6, 5, TFT_YELLOW);
    spr.fillCircle(gx, gy - 6, 2, TFT_WHITE);
  }
  spr.fillRect(gx - 4, gy - 2, 8, 26, col565(70, 70, 75));
  spr.fillRect(gx - 2, gy - 4, 4, 4, col565(35, 35, 35));
  spr.drawFastVLine(gx - 1, gy, 22, col565(140, 140, 150));
  spr.fillRect(gx - 6, gy + 8, 12, 6, col565(110, 60, 30));
  spr.fillRoundRect(gx - 13, gy + 12, 10, 14, 3, col565(230, 170, 120));
  spr.fillRoundRect(gx + 3,  gy + 14, 10, 12, 3, col565(230, 170, 120));
}

static void drawHUD() {
  spr.fillRect(0, VIEW_H, SCR_W, HUD_H, col565(45, 45, 45));
  spr.drawFastHLine(0, VIEW_H, SCR_W, col565(120, 120, 120));
  spr.setTextDatum(TL_DATUM);
  spr.setTextSize(1);
  int ty = VIEW_H + 6;
  spr.setTextColor(TFT_RED);    spr.drawString("HP", 3, ty);
  spr.setTextColor(TFT_WHITE);  spr.drawNumber(hp, 17, ty);
  spr.setTextColor(TFT_YELLOW); spr.drawString("AM", 44, ty);
  spr.setTextColor(TFT_WHITE);  spr.drawNumber(ammo, 58, ty);
  char buf[8]; snprintf(buf, sizeof(buf), "%d/%d", kills, NUM_EN);
  spr.setTextColor(TFT_GREEN);  spr.drawString("K", 88, ty);
  spr.setTextColor(TFT_WHITE);  spr.drawString(buf, 96, ty);
}

static void centerText(const char* s, int y, int size, uint16_t c) {
  spr.setTextDatum(MC_DATUM);
  spr.setTextSize(size);
  spr.setTextColor(c);
  spr.drawString(s, SCR_W / 2, y);
  spr.setTextSize(1);
  spr.setTextDatum(TL_DATUM);
}

static void render() {
  if (state == TITLE) {
    spr.fillSprite(TFT_BLACK);
    centerText("ESP32", 36, 2, TFT_RED);
    centerText("DOOM", 56, 3, TFT_RED);
    centerText("Ates = Basla", 96, 1, TFT_WHITE);
    spr.pushSprite(0, 0);
    return;
  }

  renderWorld();
  renderSprites();
  drawGun();

  uint16_t ch = TFT_GREEN;
  spr.drawFastHLine(SCR_W / 2 - 3, VIEW_H / 2, 2, ch);
  spr.drawFastHLine(SCR_W / 2 + 2, VIEW_H / 2, 2, ch);
  spr.drawFastVLine(SCR_W / 2, VIEW_H / 2 - 3, 2, ch);
  spr.drawFastVLine(SCR_W / 2, VIEW_H / 2 + 2, 2, ch);

  if (hurtT > 0 || state == DEAD) {
    for (int i = 0; i < 3; i++) spr.drawRect(i, i, SCR_W - 2 * i, VIEW_H - 2 * i, TFT_RED);
  }
  drawHUD();

  if (state == DEAD) {
    centerText("OLDUN", 40, 2, TFT_RED);
    centerText("Ates = Tekrar", 64, 1, TFT_WHITE);
  } else if (state == WIN) {
    centerText("BOLUM", 34, 2, TFT_GREEN);
    centerText("TAMAM", 52, 2, TFT_GREEN);
    centerText("Ates = Tekrar", 74, 1, TFT_WHITE);
  }
  spr.pushSprite(0, 0);
}

// ---------- Arduino ----------
static uint32_t lastUs, fpsTimer;
static int frames;

void setup() {
  Serial.begin(115200);
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);

  // Butonlar ekrandan SONRA: TFT_eSPI'nin pin ayarlarını ezmemesi için
  pinMode(BTN_FIRE,  INPUT_PULLUP);
  pinMode(BTN_FWD,   INPUT_PULLUP);
  pinMode(BTN_LEFT,  INPUT_PULLUP);
  pinMode(BTN_BACK,  INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);

  spr.setColorDepth(16);
  fb = (uint16_t*)spr.createSprite(SCR_W, SCR_H);
  if (!fb) { Serial.println("Sprite olusturulamadi!"); while (true) delay(1000); }

  randomSeed(esp_random());
  genTextures();
  resetGame();
  lastUs = micros();
  fpsTimer = millis();
}

void loop() {
  uint32_t now = micros();
  float dt = (now - lastUs) * 1e-6f;
  lastUs = now;
  if (dt > 0.05f) dt = 0.05f;

  update(dt);
  render();

  frames++;
  if (millis() - fpsTimer >= 1000) {
    Serial.printf("FPS: %d\n", frames);
    frames = 0;
    fpsTimer = millis();
  }
}
