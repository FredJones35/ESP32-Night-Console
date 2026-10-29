#include "sd.h"
#include <SD.h>
#include <SPI.h>
#include <string.h>
#include "hw.h"

namespace sd {

static SPIClass vspi(VSPI);
static bool mounted;
static bool spiStarted;

bool init() {
  if (mounted) { SD.end(); mounted = false; }
  if (!spiStarted) { vspi.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS); spiStarted = true; }
  mounted = SD.begin(PIN_SD_CS, vspi, 20000000);
  if (mounted && SD.cardType() == CARD_NONE) { SD.end(); mounted = false; }
  hwLog("SD: %s", mounted ? "takili" : "yok");
  return mounted;
}

bool ok() { return mounted; }

static bool hasExt(const char* name, const char* ext) {
  size_t n = strlen(name), e = strlen(ext);
  return n > e && strcasecmp(name + n - e, ext) == 0;
}

int list(const char* dir, const char* ext, char (*names)[SD_NAME_LEN], int max) {
  if (!mounted) return 0;
  File d = SD.open(dir);
  if (!d || !d.isDirectory()) return 0;
  int n = 0;
  for (File f = d.openNextFile(); f; f = d.openNextFile()) {
    const char* nm = f.name();
    const char* base = strrchr(nm, '/');
    base = base ? base + 1 : nm;
    if (!f.isDirectory() && base[0] != '.' && hasExt(base, ext) && strlen(base) < SD_NAME_LEN) {
      if (names && n < max) strcpy(names[n], base);
      n++;
    }
    f.close();
    if (names && n >= max) break;
  }
  d.close();
  if (names) {  // ada göre sırala (insertion sort, liste küçük)
    for (int i = 1; i < n; i++) {
      char tmp[SD_NAME_LEN];
      strcpy(tmp, names[i]);
      int j = i - 1;
      while (j >= 0 && strcasecmp(names[j], tmp) > 0) { strcpy(names[j + 1], names[j]); j--; }
      strcpy(names[j + 1], tmp);
    }
  }
  return n;
}

int count(const char* dir, const char* ext) {
  return list(dir, ext, nullptr, 0x7FFF);
}

char* load(const char* path, size_t maxLen) {
  if (!mounted) return nullptr;
  File f = SD.open(path, FILE_READ);
  if (!f) return nullptr;
  size_t len = f.size();
  if (len == 0 || len > maxLen) { f.close(); return nullptr; }
  char* buf = (char*)malloc(len + 1);
  if (!buf) { f.close(); return nullptr; }
  size_t got = f.read((uint8_t*)buf, len);
  f.close();
  if (got != len) { free(buf); return nullptr; }
  buf[len] = 0;
  return buf;
}

}  // namespace sd
