// Giriş katmanı: debounce, down/pressed/released olayları, basılı tutma süresi.
#pragma once
#include <stdint.h>

enum Btn : uint8_t { B_FIRE, B_FWD, B_LEFT, B_RIGHT, B_BACK, B_RELOAD, B_COUNT };

namespace input {

void  update(uint32_t nowMs);   // her karede bir kez
bool  down(Btn b);              // şu an basılı
bool  pressed(Btn b);           // bu karede yeni basıldı
bool  released(Btn b);          // bu karede bırakıldı
float held(Btn b);              // basılı tutma süresi (sn), basılı değilse 0
bool  repeat(Btn b, float delay = 0.25f, float rate = 0.08f);  // basış + otomatik tekrar
bool  anyPressed();
void  clear();                  // basılı butonları bırakılana kadar yok say

// SOLA + SAĞA birlikte basılı tutma süresi (ham durum, clear()'dan etkilenmez)
float comboHeld();

}  // namespace input
