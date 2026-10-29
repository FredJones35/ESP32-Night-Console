#include "hw.h"
#include <Arduino.h>
#include <stdarg.h>

static const uint8_t BTN_PINS[6] = { PIN_FIRE, PIN_FWD, PIN_LEFT, PIN_RIGHT, PIN_BACK, PIN_RELOAD };

void hwInit() {
  Serial.begin(115200);
}

void hwInitButtons() {
  for (uint8_t p : BTN_PINS) pinMode(p, INPUT_PULLUP);
}

uint8_t hwButtons() {
  uint8_t m = 0;
  for (int i = 0; i < 6; i++)
    if (digitalRead(BTN_PINS[i]) == LOW) m |= 1 << i;
  return m;
}

uint32_t hwMs()          { return millis(); }
uint32_t hwUs()          { return micros(); }
uint32_t hwRandom()      { return esp_random(); }
uint32_t hwFreeHeap()    { return ESP.getFreeHeap(); }
uint32_t hwMinFreeHeap() { return ESP.getMinFreeHeap(); }
uint32_t hwMaxAllocHeap(){ return ESP.getMaxAllocHeap(); }

void hwLog(const char* fmt, ...) {
  char buf[160];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  Serial.println(buf);
}
