#include "display.h"
#include <TFT_eSPI.h>
#include "hw.h"

static TFT_eSPI tft;
static TFT_eSprite spr(&tft);
uint16_t* fb = nullptr;

bool displayInit() {
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);
  spr.setColorDepth(16);
  fb = (uint16_t*)spr.createSprite(SCR_W, SCR_H);
  return fb != nullptr;
}

void displayPush() {
  spr.pushSprite(0, 0);
}
