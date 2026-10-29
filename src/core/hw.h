// Donanım katmanı: pinler, zaman, rastgele sayı, seri log, heap bilgisi.
// Cihazda hw.cpp (Arduino), bilgisayarda tools/sim/host.cpp tarafından uygulanır.
#pragma once
#include <stdint.h>

// Butonlar (GND'ye bağlı, INPUT_PULLUP, basılınca LOW)
#define PIN_FIRE    19
#define PIN_FWD      4
#define PIN_LEFT    32
#define PIN_RIGHT   25
#define PIN_BACK    33
#define PIN_RELOAD  22

// SD kart (VSPI)
#define PIN_SD_SCK  18
#define PIN_SD_MISO 21
#define PIN_SD_MOSI 23
#define PIN_SD_CS    5

static const int SCR_W = 128;
static const int SCR_H = 128;

void     hwInit();          // seri port
void     hwInitButtons();   // tft.init() SONRASINDA çağrılmalı
uint8_t  hwButtons();       // ham durum, bit i = buton i basılı (Btn sırası)
uint32_t hwMs();
uint32_t hwUs();
uint32_t hwRandom();
uint32_t hwFreeHeap();
uint32_t hwMinFreeHeap();
uint32_t hwMaxAllocHeap();
void     hwLog(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
