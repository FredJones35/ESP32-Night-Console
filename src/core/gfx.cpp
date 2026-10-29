#include "gfx.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "font5x7.h"

namespace gfx {

void clear(uint16_t c) {
  uint16_t s = sw16(c);
  uint32_t v = ((uint32_t)s << 16) | s;
  uint32_t* p = (uint32_t*)fb;
  for (int i = 0; i < SCR_W * SCR_H / 2; i++) p[i] = v;
}

void hline(int x, int y, int w, uint16_t c) {
  if ((unsigned)y >= (unsigned)SCR_H) return;
  if (x < 0) { w += x; x = 0; }
  if (x + w > SCR_W) w = SCR_W - x;
  if (w <= 0) return;
  uint16_t s = sw16(c);
  uint16_t* p = fb + y * SCR_W + x;
  while (w--) *p++ = s;
}

void vline(int x, int y, int h, uint16_t c) {
  if ((unsigned)x >= (unsigned)SCR_W) return;
  if (y < 0) { h += y; y = 0; }
  if (y + h > SCR_H) h = SCR_H - y;
  if (h <= 0) return;
  uint16_t s = sw16(c);
  uint16_t* p = fb + y * SCR_W + x;
  while (h--) { *p = s; p += SCR_W; }
}

void fillRect(int x, int y, int w, int h, uint16_t c) {
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > SCR_W) w = SCR_W - x;
  if (y + h > SCR_H) h = SCR_H - y;
  if (w <= 0 || h <= 0) return;
  uint16_t s = sw16(c);
  for (int j = 0; j < h; j++) {
    uint16_t* p = fb + (y + j) * SCR_W + x;
    for (int i = 0; i < w; i++) p[i] = s;
  }
}

void rect(int x, int y, int w, int h, uint16_t c) {
  hline(x, y, w, c); hline(x, y + h - 1, w, c);
  vline(x, y, h, c); vline(x + w - 1, y, h, c);
}

void line(int x0, int y0, int x1, int y1, uint16_t c) {
  int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  for (int guard = 0; guard < 600; guard++) {
    pixel(x0, y0, c);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}

void thickLine(int x0, int y0, int x1, int y1, int t, uint16_t c) {
  if (t <= 1) { line(x0, y0, x1, y1, c); return; }
  int dx = abs(x1 - x0), dy = abs(y1 - y0);
  int o = -(t / 2);
  for (int i = 0; i < t; i++) {
    if (dx > dy) line(x0, y0 + o + i, x1, y1 + o + i, c);
    else         line(x0 + o + i, y0, x1 + o + i, y1, c);
  }
}

void circle(int cx, int cy, int r, uint16_t c) {
  int x = r, y = 0, err = 1 - r;
  while (x >= y) {
    pixel(cx + x, cy + y, c); pixel(cx - x, cy + y, c);
    pixel(cx + x, cy - y, c); pixel(cx - x, cy - y, c);
    pixel(cx + y, cy + x, c); pixel(cx - y, cy + x, c);
    pixel(cx + y, cy - x, c); pixel(cx - y, cy - x, c);
    y++;
    if (err < 0) err += 2 * y + 1;
    else { x--; err += 2 * (y - x) + 1; }
  }
}

void fillCircle(int cx, int cy, int r, uint16_t c) {
  if (r <= 0) { pixel(cx, cy, c); return; }
  for (int y = -r; y <= r; y++) {
    int w = 0;
    while ((w + 1) * (w + 1) + y * y <= r * r + r) w++;
    hline(cx - w, cy + y, 2 * w + 1, c);
  }
}

void fillEllipse(int cx, int cy, int rx, int ry, uint16_t c) {
  if (rx <= 0 || ry <= 0) { hline(cx - rx, cy, 2 * rx + 1, c); return; }
  for (int y = -ry; y <= ry; y++) {
    float f = 1.0f - (float)(y * y) / (float)(ry * ry);
    int w = (int)(rx * __builtin_sqrtf(f > 0 ? f : 0) + 0.5f);
    hline(cx - w, cy + y, 2 * w + 1, c);
  }
}

void fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, uint16_t c) {
  // y'ye göre sırala
  if (y0 > y1) { int t = y0; y0 = y1; y1 = t; t = x0; x0 = x1; x1 = t; }
  if (y1 > y2) { int t = y1; y1 = y2; y2 = t; t = x1; x1 = x2; x2 = t; }
  if (y0 > y1) { int t = y0; y0 = y1; y1 = t; t = x0; x0 = x1; x1 = t; }
  if (y2 == y0) {
    int a = x0 < x1 ? x0 : x1, b = x0 > x1 ? x0 : x1;
    a = a < x2 ? a : x2;
    b = b > x2 ? b : x2;
    hline(a, y0, b - a + 1, c);
    return;
  }
  int ys = y0 < 0 ? 0 : y0, ye = y2 >= SCR_H ? SCR_H - 1 : y2;
  for (int y = ys; y <= ye; y++) {
    int xa = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
    int xb;
    if (y < y1) xb = (y1 == y0) ? x1 : x0 + (x1 - x0) * (y - y0) / (y1 - y0);
    else        xb = (y2 == y1) ? x1 : x1 + (x2 - x1) * (y - y1) / (y2 - y1);
    if (xa > xb) { int t = xa; xa = xb; xb = t; }
    hline(xa, y, xb - xa + 1, c);
  }
}

void darken(int x, int y, int w, int h, int f) {
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > SCR_W) w = SCR_W - x;
  if (y + h > SCR_H) h = SCR_H - y;
  for (int j = 0; j < h; j++) {
    uint16_t* p = fb + (y + j) * SCR_W + x;
    for (int i = 0; i < w; i++) p[i] = sw16(shade(sw16(p[i]), f));
  }
}

void blend(int x, int y, int w, int h, uint16_t c, int t) {
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > SCR_W) w = SCR_W - x;
  if (y + h > SCR_H) h = SCR_H - y;
  for (int j = 0; j < h; j++) {
    uint16_t* p = fb + (y + j) * SCR_W + x;
    for (int i = 0; i < w; i++) p[i] = sw16(mix(sw16(p[i]), c, t));
  }
}

void vgradient(int x, int y, int w, int h, uint16_t top, uint16_t bottom) {
  for (int j = 0; j < h; j++) hline(x, y + j, w, mix(top, bottom, h > 1 ? j * 256 / (h - 1) : 0));
}

int textWidth(const char* s, int scale) {
  int n = (int)strlen(s);
  return n ? n * 6 * scale - scale : 0;
}

int text(int x, int y, const char* s, uint16_t c, int scale, Align al) {
  int w = textWidth(s, scale);
  if (al == AL_CENTER) x -= w / 2;
  else if (al == AL_RIGHT) x -= w;
  int cx = x;
  for (; *s; s++) {
    unsigned char ch = (unsigned char)*s;
    if (ch < 0x20 || ch > 0x7E) ch = '?';
    const uint8_t* g = FONT5X7 + (ch - 0x20) * 5;
    for (int col = 0; col < 5; col++) {
      uint8_t bits = g[col];
      for (int row = 0; row < 8; row++) {
        if (!(bits & (1 << row))) continue;
        if (scale == 1) pixel(cx + col, y + row, c);
        else fillRect(cx + col * scale, y + row * scale, scale, scale, c);
      }
    }
    cx += 6 * scale;
  }
  return w;
}

int textShadow(int x, int y, const char* s, uint16_t c, int scale, Align al, uint16_t sh) {
  text(x + 1, y + 1, s, sh, scale, al);
  return text(x, y, s, c, scale, al);
}

int textf(int x, int y, uint16_t c, int scale, Align al, const char* fmt, ...) {
  char buf[64];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  return text(x, y, buf, c, scale, al);
}

void art(int x, int y, const char* const* rows, int w, int h, int scale, PalFn pal, bool flip) {
  for (int j = 0; j < h; j++) {
    const char* r = rows[j];
    for (int i = 0; i < w; i++) {
      char ch = r[flip ? w - 1 - i : i];
      if (ch == '.' || ch == ' ') continue;
      if (scale == 1) pixel(x + i, y + j, pal(ch));
      else fillRect(x + i * scale, y + j * scale, scale, scale, pal(ch));
    }
  }
}

}  // namespace gfx
