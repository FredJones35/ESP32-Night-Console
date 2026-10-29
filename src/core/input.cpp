#include "input.h"
#include "hw.h"

namespace input {

static const uint32_t DEBOUNCE_MS = 25;   // durum değişiminden sonra kilit süresi

static uint8_t  state;        // debounce edilmiş durum
static uint8_t  prevState;
static uint8_t  suppressed;   // clear() sonrası bırakılana kadar yok sayılanlar
static uint32_t lockUntil[B_COUNT];
static uint32_t downSince[B_COUNT];
static uint16_t repeatCount[B_COUNT];
static uint32_t now;

void update(uint32_t nowMs) {
  now = nowMs;
  uint8_t raw = hwButtons();
  prevState = state;
  for (int i = 0; i < B_COUNT; i++) {
    uint8_t bit = 1 << i;
    if (((raw ^ state) & bit) && (int32_t)(now - lockUntil[i]) >= 0) {
      state ^= bit;
      lockUntil[i] = now + DEBOUNCE_MS;
      if (state & bit) { downSince[i] = now; repeatCount[i] = 0; }
    }
  }
  suppressed &= state;   // bırakılanlar artık serbest
}

static inline bool rawDown(Btn b) { return state & (1 << b); }

bool down(Btn b)     { return rawDown(b) && !(suppressed & (1 << b)); }
bool pressed(Btn b)  { return (state & ~prevState & ~suppressed) & (1 << b); }
bool released(Btn b) { return (~state & prevState) & (1 << b); }

float held(Btn b) {
  if (!down(b)) return 0;
  return (now - downSince[b]) * 0.001f;
}

bool repeat(Btn b, float delay, float rate) {
  if (pressed(b)) return true;
  if (!down(b)) return false;
  float h = held(b);
  if (h < delay) return false;
  uint16_t n = (uint16_t)((h - delay) / rate) + 1;
  if (n != repeatCount[b]) { repeatCount[b] = n; return true; }
  return false;
}

bool anyPressed() {
  return (state & ~prevState & ~suppressed) != 0;
}

void clear() {
  suppressed = state;
  prevState = state;
}

float comboHeld() {
  if (!rawDown(B_LEFT) || !rawDown(B_RIGHT)) return 0;
  uint32_t since = downSince[B_LEFT] > downSince[B_RIGHT] ? downSince[B_LEFT] : downSince[B_RIGHT];
  return (now - since) * 0.001f;
}

}  // namespace input
