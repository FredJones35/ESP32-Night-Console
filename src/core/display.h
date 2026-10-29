// Ekran: tek, paylaşılan 128x128 RGB565 framebuffer (TFT_eSprite, 32 KB).
// fb içindeki pikseller bayt sırası çevrilmiş (byte-swapped) RGB565'tir.
#pragma once
#include <stdint.h>

extern uint16_t* fb;

bool displayInit();   // false: framebuffer ayrılamadı
void displayPush();   // framebuffer'ı ekrana gönder
