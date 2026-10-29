// SD kart (isteğe bağlı). Kendi SPIClass(VSPI) nesnesiyle çalışır.
// Kart yoksa ya da okunamıyorsa tüm fonksiyonlar güvenle boş döner.
#pragma once
#include <stddef.h>

static const int SD_NAME_LEN = 32;

namespace sd {

bool  init();     // kartı bağlamayı dener (yeniden tarama için tekrar çağrılabilir)
bool  ok();
int   count(const char* dir, const char* ext);  // ör. ("/doom", ".lvl")
// dir içindeki ext uzantılı dosyaları ada göre sıralı listeler, en fazla max adet
int   list(const char* dir, const char* ext, char (*names)[SD_NAME_LEN], int max);
// Dosyanın tamamını sonuna '\0' eklenmiş malloc'lu tampona okur. free() ile bırakılır.
char* load(const char* path, size_t maxLen = 16384);

}  // namespace sd
