#include "textures.h"
#include <math.h>
#include "../../core/font5x7.h"
#include "../../core/gfx.h"

namespace doom {

static inline int noise(int x, int y, uint32_t seed) {
  return (int)(hash32((uint32_t)x * 73856093u ^ (uint32_t)y * 19349663u ^ seed) & 255);
}

// Pürüzsüz değer gürültüsü (0..255), periyot 32'de sarar
static int smooth(float x, float y, int cell, uint32_t seed) {
  int x0 = (int)floorf(x / cell), y0 = (int)floorf(y / cell);
  float fx = x / cell - x0, fy = y / cell - y0;
  int n = TEX / cell;
  auto v = [&](int a, int b) { return (float)noise(((a % n) + n) % n, ((b % n) + n) % n, seed); };
  float a = v(x0, y0) + (v(x0 + 1, y0) - v(x0, y0)) * fx;
  float b = v(x0, y0 + 1) + (v(x0 + 1, y0 + 1) - v(x0, y0 + 1)) * fx;
  return (int)(a + (b - a) * fy);
}

// Düzensiz kan lekesi ve aşağı akan damlalar
static void splat(uint16_t* t, int cx, int cy, int r, uint32_t seed) {
  for (int y = -r; y <= r; y++)
    for (int x = -r; x <= r; x++) {
      int d2 = x * x + y * y;
      int jitter = noise(x + cx, y + cy, seed) % (r + 1);
      if (d2 > r * r + jitter * 2) continue;
      int px = (cx + x) & (TEX - 1), py = cy + y;
      if (py < 0 || py >= TEX) continue;
      int dark = noise(px, py, seed + 7) & 63;
      t[py * TEX + px] = rgb(120 + dark, 0, 0);
    }
  for (int i = 0; i < 3; i++) {
    int dx = cx + (int)(noise(i, 1, seed) % (2 * r + 1)) - r;
    int len = 3 + noise(i, 2, seed) % 10;
    for (int y = cy; y < cy + r + len && y < TEX; y++)
      t[y * TEX + (dx & (TEX - 1))] = rgb(110 + (y & 3) * 10, 0, 0);
  }
}

static void brick(uint16_t* t) {
  for (int y = 0; y < TEX; y++)
    for (int x = 0; x < TEX; x++) {
      int nz = noise(x, y, 1) / 8 - 16;
      int off = ((y / 8) & 1) * 8;
      bool mortar = (y % 8 == 7) || ((x + off) % 16 == 15);
      t[y * TEX + x] = mortar ? rgb(45 + nz / 2, 35 + nz / 2, 32 + nz / 2)
                              : rgb(105 + nz, 45 + nz / 2, 35 + nz / 2);
    }
  splat(t, 9, 6, 4, 11);
  splat(t, 24, 19, 3, 12);
}

static void metal(uint16_t* t) {
  for (int y = 0; y < TEX; y++)
    for (int x = 0; x < TEX; x++) {
      int nz = noise(x, y, 2) / 10 - 12;
      int rust = smooth((float)x, (float)y, 8, 21);
      bool edge  = (x % 16 == 0) || (y % 16 == 0);
      bool rivet = ((x % 16 == 3) || (x % 16 == 13)) && ((y % 16 == 3) || (y % 16 == 13));
      uint16_t c;
      if (edge)        c = rgb(25, 22, 22);
      else if (rivet)  c = rgb(150, 140, 130);
      else if (rust > 170) c = rgb(110 + nz, 55 + nz / 2, 25);
      else             c = rgb(75 + nz, 72 + nz, 78 + nz);
      t[y * TEX + x] = c;
    }
  // kanlı el izi gibi sürtme
  for (int y = 10; y < 30; y++)
    for (int x = 18; x < 24; x++)
      if (noise(x, y, 33) > 90 + (y - 10) * 6) t[y * TEX + x] = rgb(130, 5, 5);
}

static void flesh(uint16_t* t) {
  for (int y = 0; y < TEX; y++)
    for (int x = 0; x < TEX; x++) {
      int a = smooth((float)x, (float)y, 8, 3);
      int b = smooth((float)x * 1.0f, (float)y, 4, 4);
      int v = (a * 2 + b) / 3;
      uint16_t c = rgb(90 + v / 2, 20 + v / 8, 25 + v / 8);
      if (abs(v - 128) < 7) c = rgb(60, 0, 10);         // damar
      if (abs(b - 140) < 4) c = rgb(170, 40, 50);       // parlak doku
      t[y * TEX + x] = c;
    }
}

static void stone(uint16_t* t) {
  for (int y = 0; y < TEX; y++)
    for (int x = 0; x < TEX; x++) {
      int bx = (x + ((y / 10) & 1) * 5) / 11, by = y / 10;
      int base = 55 + (int)(hash32(bx * 7 + by * 131 + 5) % 30);
      int nz = noise(x, y, 5) / 12 - 10;
      bool seam = (y % 10 == 9) || ((x + ((y / 10) & 1) * 5) % 11 == 10);
      t[y * TEX + x] = seam ? rgb(15, 12, 12) : rgb(base + nz, base + nz - 5, base + nz - 4);
    }
  // kafatası nişi
  static const char* const SKULL[] = {
    ".WWWW.", "WWWWWW", "WKWWKW", "WKWWKW", "WWWWWW", ".WKKW.", ".W.W.W",
  };
  for (int y = 0; y < 7; y++)
    for (int x = 0; x < 6; x++) {
      char ch = SKULL[y][x];
      if (ch == '.') continue;
      t[(12 + y) * TEX + 13 + x] = ch == 'W' ? rgb(190, 180, 160) : rgb(10, 0, 0);
    }
  splat(t, 6, 25, 2, 55);
}

static void hell(uint16_t* t) {
  for (int y = 0; y < TEX; y++)
    for (int x = 0; x < TEX; x++) {
      int nz = noise(x, y, 8) / 12;
      int blk = (int)(hash32((uint32_t)(x / 6) * 31 + (uint32_t)(y / 5) * 17 + 8) % 14);
      t[y * TEX + x] = rgb(26 + nz + blk, 20 + nz + blk / 2, 20 + nz);
    }
  // rastgele yürüyüşle lav çatlakları
  for (int c = 0; c < 5; c++) {
    int x = noise(c, 0, 40) % TEX, y = noise(c, 1, 40) % TEX;
    for (int i = 0; i < 26; i++) {
      int r = noise(c, i, 41) % 4;
      x = (x + (r == 0 ? 1 : r == 1 ? -1 : 0) + TEX) % TEX;
      y = (y + (r >= 2 ? 1 : 0)) % TEX;
      t[y * TEX + x] = rgb(255, 90 + (i & 3) * 30, 0);
      int nx = (x + 1) % TEX;
      if ((t[y * TEX + nx] >> 11) < 20) t[y * TEX + nx] = rgb(120, 15, 0);
    }
  }
}

static void tech(uint16_t* t) {
  for (int y = 0; y < TEX; y++)
    for (int x = 0; x < TEX; x++) {
      int nz = noise(x, y, 9) / 16 - 8;
      uint16_t c = rgb(48 + nz, 46 + nz, 52 + nz);
      if (x == 0 || x == 31 || y == 0 || y == 31) c = rgb(20, 20, 22);
      if ((y == 8 || y == 24) && x > 3 && x < 28) c = rgb(20, 18, 20);
      if (y >= 14 && y <= 17 && x > 3 && x < 28) c = (x & 3) ? rgb(220, 20, 10) : rgb(90, 0, 0);
      if ((x == 6 || x == 25) && y > 2 && y < 6) c = rgb(110, 110, 120);
      t[y * TEX + x] = c;
    }
  splat(t, 20, 26, 2, 66);
}

static void exitTex(uint16_t* t) {
  for (int y = 0; y < TEX; y++)
    for (int x = 0; x < TEX; x++) {
      int nz = noise(x, y, 10) / 16 - 8;
      t[y * TEX + x] = rgb(40 + nz, 38 + nz, 40 + nz);
    }
  for (int y = 4; y < 14; y++)
    for (int x = 1; x < 31; x++) t[y * TEX + x] = rgb(25, 0, 0);
  const char* s = "CIKIS";
  for (int i = 0; i < 5; i++) {
    const uint8_t* g = FONT5X7 + (s[i] - 0x20) * 5;
    for (int c = 0; c < 5; c++)
      for (int r = 0; r < 7; r++)
        if (g[c] & (1 << r)) t[(5 + r) * TEX + 2 + i * 6 + c] = rgb(255, 40, 20);
  }
  // şalter
  for (int y = 17; y < 29; y++)
    for (int x = 12; x < 20; x++) t[y * TEX + x] = rgb(70, 70, 80);
  for (int y = 19; y < 27; y++) t[y * TEX + 15] = t[y * TEX + 16] = rgb(200, 30, 20);
}

static void door(uint16_t* t, uint16_t band) {
  for (int y = 0; y < TEX; y++)
    for (int x = 0; x < TEX; x++) {
      int nz = noise(x, y, 11) / 16 - 8;
      uint16_t c = rgb(85 + nz, 80 + nz, 75 + nz);
      if (x == 15 || x == 16) c = rgb(20, 18, 18);                    // orta ek yeri
      if (x < 2 || x > 29) c = rgb(40, 38, 36);                        // kasa
      if (y >= 26 && x >= 2 && x <= 29) c = ((x + y) / 3) & 1 ? rgb(200, 160, 0) : rgb(20, 20, 20);
      if (y >= 6 && y <= 11 && x > 3 && x < 28) c = band;
      t[y * TEX + x] = c;
    }
}

void genTextures(uint16_t* tex) {
  brick(tex + T_BRICK * TEX * TEX);
  metal(tex + T_METAL * TEX * TEX);
  flesh(tex + T_FLESH * TEX * TEX);
  stone(tex + T_STONE * TEX * TEX);
  hell(tex + T_HELL * TEX * TEX);
  tech(tex + T_TECH * TEX * TEX);
  exitTex(tex + T_EXIT * TEX * TEX);
  door(tex + T_DOOR * TEX * TEX, rgb(60, 58, 56));
  door(tex + T_DOOR_R * TEX * TEX, rgb(220, 20, 20));
  door(tex + T_DOOR_Y * TEX * TEX, rgb(230, 200, 0));
  door(tex + T_DOOR_B * TEX * TEX, rgb(30, 70, 230));
}

}  // namespace doom
