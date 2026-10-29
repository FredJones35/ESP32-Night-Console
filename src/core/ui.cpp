#include "ui.h"
#include <math.h>
#include <stdio.h>
#include "display.h"
#include "gfx.h"
#include "input.h"

namespace ui {

void panel(int x, int y, int w, int h, uint16_t border, uint16_t fill) {
  gfx::fillRect(x, y, w, h, fill);
  gfx::rect(x, y, w, h, border);
  gfx::rect(x + 1, y + 1, w - 2, h - 2, shade(border, 110));
}

void bar(int x, int y, int w, int h, float frac, uint16_t c, uint16_t bg) {
  if (frac < 0) frac = 0;
  if (frac > 1) frac = 1;
  gfx::fillRect(x, y, w, h, bg);
  gfx::fillRect(x, y, (int)(w * frac + 0.5f), h, c);
}

void confirmBox(const char* title, const char* line1, const char* line2) {
  int h = line2 ? 56 : 46;
  int y = (SCR_H - h) / 2;
  panel(8, y, SCR_W - 16, h);
  gfx::textShadow(SCR_W / 2, y + 7, title, C_WHITE, 1, AL_CENTER);
  gfx::text(SCR_W / 2, y + 21, line1, rgb(255, 200, 60), 1, AL_CENTER);
  if (line2) gfx::text(SCR_W / 2, y + 33, line2, C_GREY, 1, AL_CENTER);
}

void banner(const char* title, const char* sub, uint16_t c) {
  int w = gfx::textWidth(title, 2);
  int bw = (w > 100 ? w : 100) + 12;
  if (bw > SCR_W - 4) bw = SCR_W - 4;
  int h = sub ? 38 : 26;
  int y = 40;
  gfx::darken((SCR_W - bw) / 2, y, bw, h, 70);
  gfx::rect((SCR_W - bw) / 2, y, bw, h, shade(c, 150));
  gfx::textShadow(SCR_W / 2, y + 6, title, c, 2, AL_CENTER);
  if (sub) gfx::text(SCR_W / 2, y + 26, sub, C_WHITE, 1, AL_CENTER);
}

void controlsScreen(const Game* g, float t) {
  gfx::vgradient(0, 0, SCR_W, SCR_H, rgb(30, 0, 0), C_BLACK);
  gfx::textShadow(SCR_W / 2, 4, g->name, rgb(255, 60, 40), 1, AL_CENTER);
  gfx::text(SCR_W / 2, 14, "- KONTROLLER -", C_GREY, 1, AL_CENTER);
  int y = 28;
  for (const char* const* c = g->controls; c && c[0] && c[1] && y < 100; c += 2, y += 10) {
    gfx::text(3, y, c[0], rgb(255, 200, 60));
    gfx::text(50, y, c[1], C_WHITE);
  }
  gfx::hline(4, 103, SCR_W - 8, rgb(80, 0, 0));
  gfx::text(SCR_W / 2, 107, "SOL+SAG 1.5sn: Menu", C_GREY, 1, AL_CENTER);
  if (fmodf(t, 1.0f) < 0.65f) gfx::text(SCR_W / 2, 118, "ATES: Basla", rgb(255, 80, 60), 1, AL_CENTER);
}

void loadingScreen(const char* what) {
  gfx::clear(C_BLACK);
  gfx::text(SCR_W / 2, 56, what, C_WHITE, 1, AL_CENTER);
  gfx::text(SCR_W / 2, 68, "Yukleniyor...", C_GREY, 1, AL_CENTER);
  displayPush();
}

void errorScreen(const char* msg) {
  gfx::clear(C_BLACK);
  gfx::text(SCR_W / 2, 50, "HATA", C_RED, 2, AL_CENTER);
  gfx::text(SCR_W / 2, 72, msg, C_WHITE, 1, AL_CENTER);
  displayPush();
}

void formatTime(char* out, int n, uint32_t ms) {
  uint32_t cs = ms / 10;
  snprintf(out, n, "%u:%02u.%02u", (unsigned)(cs / 6000), (unsigned)((cs / 100) % 60), (unsigned)(cs % 100));
}

int List::update(int count, float dt, int visibleRows) {
  if (count <= 0) return input::pressed(B_RELOAD) ? -2 : -1;
  if (input::repeat(B_FWD) && sel > 0) sel--;
  if (input::repeat(B_BACK) && sel < count - 1) sel++;
  if (sel >= count) sel = count - 1;
  float target = scroll;
  if (sel < target) target = (float)sel;
  if (sel > target + visibleRows - 1) target = (float)(sel - visibleRows + 1);
  float maxTop = (float)(count > visibleRows ? count - visibleRows : 0);
  if (target > maxTop) target = maxTop;
  if (target < 0) target = 0;
  float k = dt * 14.0f;
  if (k > 1) k = 1;
  scroll += (target - scroll) * k;
  if (fabsf(target - scroll) < 0.01f) scroll = target;
  if (input::pressed(B_FIRE)) return sel;
  if (input::pressed(B_RELOAD)) return -2;
  return -1;
}

void listDraw(List& l, int count, int x, int y, int w, int rowH, int visibleRows, RowFn row, float t) {
  // Liste alanı dışına taşan satırları çağıran, başlık/alt bilgiyi sonra çizerek örter.
  int first = (int)floorf(l.scroll);
  for (int i = first; i < count && i <= first + visibleRows; i++) {
    int ry = y + (int)lroundf((i - l.scroll) * rowH);
    row(i, x, ry, w, rowH, i == l.sel, t);
  }
  // kaydırma çubuğu
  if (count > visibleRows) {
    int th = visibleRows * rowH;
    int bh = th * visibleRows / count;
    int by = y + (int)((th - bh) * (l.scroll / (count - visibleRows)));
    gfx::vline(x + w - 2, y, th, rgb(40, 0, 0));
    gfx::fillRect(x + w - 3, by, 3, bh, rgb(180, 20, 20));
  }
}

}  // namespace ui
