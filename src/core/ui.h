// Ortak arayüz parçaları: paneller, onay kutusu, liste, kontroller ekranı.
#pragma once
#include <stdint.h>
#include "game.h"

namespace ui {

static const uint16_t ACCENT = 0xC000;   // koyu kırmızı vurgu

void panel(int x, int y, int w, int h, uint16_t border = ACCENT, uint16_t fill = 0x0841);
void bar(int x, int y, int w, int h, float frac, uint16_t c, uint16_t bg = 0x2104);
void confirmBox(const char* title, const char* line1, const char* line2 = nullptr);
void banner(const char* title, const char* sub, uint16_t c);   // ekran ortasında büyük yazı
void controlsScreen(const Game* g, float t);
void loadingScreen(const char* what);
void errorScreen(const char* msg);
void formatTime(char* out, int n, uint32_t ms);                // "1:23.45"

// Dikey kaydırmalı liste
struct List {
  int sel = 0;
  float scroll = 0;   // yumuşak kaydırma
  // FWD/BACK gezinme. Dönüş: seçilen indeks (ATES), -2 (RELOAD = geri), -1 (yok)
  int update(int count, float dt, int visibleRows);
};
typedef void (*RowFn)(int i, int x, int y, int w, int h, bool selected, float t);
void listDraw(List& l, int count, int x, int y, int w, int rowH, int visibleRows, RowFn row, float t);

}  // namespace ui
