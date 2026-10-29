// Kalıcı kayıt: Preferences (NVS). Anahtarlar en fazla 15 karakter.
#pragma once
#include <stdint.h>

namespace save {

void    init();
int32_t get(const char* key, int32_t def = 0);
void    set(const char* key, int32_t value);   // değer aynıysa yazmaz
bool    setMax(const char* key, int32_t value); // daha büyükse yaz, yazdıysa true
bool    setMin(const char* key, int32_t value); // daha küçükse (ya da kayıt yoksa) yaz
void    reset();                                // tüm kayıtları sil

// Bölüm/pist adına göre anahtar: prefix + 8 haneli hash (ör. "mxt1a2b3c4d")
void    keyFor(char* out, const char* prefix, const char* name);

}  // namespace save
