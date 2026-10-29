// ESP32 Gece Konsolu — açılış menüsü + 6 oyun
// Donanım: ESP32 DevKit V1, 1.44" 128x128 ST7735 (HSPI), 6 buton, isteğe bağlı SD kart (VSPI)
#include <Arduino.h>
#include "core/game.h"

void setup() { app::init(); }
void loop()  { app::frame(); }
