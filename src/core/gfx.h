// Çizim ve yazı yardımcıları. Hepsi paylaşılan framebuffer'a (fb) çizer,
// ekran dışına taşan kısımları kırpar. Renkler normal RGB565 olarak verilir.
#pragma once
#include <stdint.h>
#include "hw.h"
#include "display.h"

static inline uint16_t sw16(uint16_t c) { return (uint16_t)((c >> 8) | (c << 8)); }
static inline uint8_t  clamp8(int v) { return v < 0 ? 0 : (v > 255 ? 255 : (uint8_t)v); }
static inline uint16_t rgb(int r, int g, int b) {
  return (uint16_t)(((clamp8(r) & 0xF8) << 8) | ((clamp8(g) & 0xFC) << 3) | (clamp8(b) >> 3));
}
static inline int red8(uint16_t c)   { return ((c >> 11) & 31) << 3; }
static inline int green8(uint16_t c) { return ((c >> 5) & 63) << 2; }
static inline int blue8(uint16_t c)  { return (c & 31) << 3; }

// f: 0..256 parlaklık çarpanı
static inline uint16_t shade(uint16_t c, int f) {
  if (f >= 256) return c;
  if (f <= 0) return 0;
  uint16_t r = (uint16_t)((((c >> 11) & 31) * f) >> 8);
  uint16_t g = (uint16_t)((((c >> 5) & 63) * f) >> 8);
  uint16_t b = (uint16_t)(((c & 31) * f) >> 8);
  return (uint16_t)((r << 11) | (g << 5) | b);
}
// t: 0..256, 0 = a, 256 = b
static inline uint16_t mix(uint16_t a, uint16_t b, int t) {
  if (t <= 0) return a;
  if (t >= 256) return b;
  int ra = (a >> 11) & 31, ga = (a >> 5) & 63, ba = a & 31;
  int rb = (b >> 11) & 31, gb = (b >> 5) & 63, bb = b & 31;
  return (uint16_t)(((ra + (((rb - ra) * t) >> 8)) << 11) |
                    ((ga + (((gb - ga) * t) >> 8)) << 5) |
                    (ba + (((bb - ba) * t) >> 8)));
}
// Doygun toplama
static inline uint16_t addc(uint16_t a, uint16_t b) {
  int r = ((a >> 11) & 31) + ((b >> 11) & 31); if (r > 31) r = 31;
  int g = ((a >> 5) & 63) + ((b >> 5) & 63);   if (g > 63) g = 63;
  int bl = (a & 31) + (b & 31);                if (bl > 31) bl = 31;
  return (uint16_t)((r << 11) | (g << 5) | bl);
}

static inline uint32_t hash32(uint32_t x) {
  x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
  return x;
}

// Sık kullanılan renkler
static const uint16_t C_BLACK  = 0x0000;
static const uint16_t C_WHITE  = 0xFFFF;
static const uint16_t C_RED    = 0xF800;
static const uint16_t C_GREEN  = 0x07E0;
static const uint16_t C_BLUE   = 0x001F;
static const uint16_t C_YELLOW = 0xFFE0;
static const uint16_t C_ORANGE = 0xFC00;
static const uint16_t C_CYAN   = 0x07FF;
static const uint16_t C_GREY   = 0x8410;
static const uint16_t C_DGREY  = 0x39E7;

enum Align : uint8_t { AL_LEFT, AL_CENTER, AL_RIGHT };

namespace gfx {

void clear(uint16_t c);
static inline void pixel(int x, int y, uint16_t c) {
  if ((unsigned)x < (unsigned)SCR_W && (unsigned)y < (unsigned)SCR_H) fb[y * SCR_W + x] = sw16(c);
}
static inline uint16_t getPixel(int x, int y) {
  if ((unsigned)x < (unsigned)SCR_W && (unsigned)y < (unsigned)SCR_H) return sw16(fb[y * SCR_W + x]);
  return 0;
}
void hline(int x, int y, int w, uint16_t c);
void vline(int x, int y, int h, uint16_t c);
void fillRect(int x, int y, int w, int h, uint16_t c);
void rect(int x, int y, int w, int h, uint16_t c);
void line(int x0, int y0, int x1, int y1, uint16_t c);
void thickLine(int x0, int y0, int x1, int y1, int t, uint16_t c);
void circle(int cx, int cy, int r, uint16_t c);
void fillCircle(int cx, int cy, int r, uint16_t c);
void fillEllipse(int cx, int cy, int rx, int ry, uint16_t c);
void fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, uint16_t c);
void darken(int x, int y, int w, int h, int f);              // f: 0..256
void blend(int x, int y, int w, int h, uint16_t c, int t);   // t: 0..256
void vgradient(int x, int y, int w, int h, uint16_t top, uint16_t bottom);

// Yazı: 5x7 font, karakter adımı 6*scale piksel
int  text(int x, int y, const char* s, uint16_t c, int scale = 1, Align al = AL_LEFT);
int  textShadow(int x, int y, const char* s, uint16_t c, int scale = 1, Align al = AL_LEFT,
                uint16_t sh = C_BLACK);
int  textf(int x, int y, uint16_t c, int scale, Align al, const char* fmt, ...)
     __attribute__((format(printf, 6, 7)));
int  textWidth(const char* s, int scale = 1);

// ASCII sanatı: rows[h] satır, her karakter pal(ch) ile renge çevrilir; '.' saydam.
typedef uint16_t (*PalFn)(char ch);
void art(int x, int y, const char* const* rows, int w, int h, int scale, PalFn pal, bool flip = false);

}  // namespace gfx
