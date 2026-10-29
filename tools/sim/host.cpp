// Bilgisayarda çalışan basit simülatör: core donanım katmanını taklit eder,
// betikle verilen buton dizisini oynatır ve istenen karelerin ekran görüntüsünü kaydeder.
//   ./sim "-*30 F*2 shot:menu U*10 shot:liste"
// Buton harfleri: F=ATES U=ILERI L=SOLA R=SAGA D=GERI X=RELOAD, '-' = hiçbiri
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <chrono>
#include <map>
#include <string>
#include <vector>
#include <algorithm>
#include <dirent.h>
#include <sys/stat.h>
#include "core/display.h"
#include "core/game.h"
#include "core/hw.h"
#include "core/save.h"
#include "core/sd.h"

static uint64_t simUs = 0;
static uint8_t simButtons = 0;
static uint16_t fbMem[128 * 128];
uint16_t* fb = fbMem;
static std::string sdRoot = "sd";
static bool sdMounted = false;
static std::map<std::string, int32_t> store;
static double frameCpuMs = 0, maxCpuMs = 0;

void hwInit() {}
void hwInitButtons() {}
uint8_t hwButtons() { return simButtons; }
uint32_t hwMs() { return (uint32_t)(simUs / 1000); }
uint32_t hwUs() { return (uint32_t)simUs; }
uint32_t hwRandom() { return (uint32_t)rand() * 2654435761u ^ (uint32_t)rand(); }
uint32_t hwFreeHeap() { return 200000; }
uint32_t hwMinFreeHeap() { return 200000; }
uint32_t hwMaxAllocHeap() { return 110000; }
void hwLog(const char* fmt, ...) {
  if (getenv("SIM_QUIET")) return;
  va_list ap; va_start(ap, fmt); vprintf(fmt, ap); va_end(ap); printf("\n");
}

bool displayInit() { return true; }
void displayPush() {}

namespace save {
void init() {}
int32_t get(const char* k, int32_t def) { auto it = store.find(k); return it == store.end() ? def : it->second; }
void set(const char* k, int32_t v) { store[k] = v; }
bool setMax(const char* k, int32_t v) { auto it = store.find(k); if (it != store.end() && it->second >= v) return false; store[k] = v; return true; }
bool setMin(const char* k, int32_t v) { auto it = store.find(k); if (it != store.end() && it->second <= v) return false; store[k] = v; return true; }
void reset() { store.clear(); }
void keyFor(char* out, const char* prefix, const char* name) {
  uint32_t h = 2166136261u;
  for (const char* p = name; *p; p++) { h ^= (uint8_t)*p; h *= 16777619u; }
  snprintf(out, 16, "%.6s%08x", prefix, (unsigned)h);
}
}

namespace sd {
bool init() { struct stat st; sdMounted = getenv("SIM_NOSD") == nullptr && stat(sdRoot.c_str(), &st) == 0; return sdMounted; }
bool ok() { return sdMounted; }
static bool hasExt(const char* n, const char* e) { size_t a = strlen(n), b = strlen(e); return a > b && strcasecmp(n + a - b, e) == 0; }
int list(const char* dir, const char* ext, char (*names)[SD_NAME_LEN], int max) {
  if (!sdMounted) return 0;
  DIR* d = opendir((sdRoot + dir).c_str());
  if (!d) return 0;
  std::vector<std::string> v;
  while (dirent* e = readdir(d)) if (e->d_name[0] != '.' && hasExt(e->d_name, ext)) v.push_back(e->d_name);
  closedir(d);
  std::sort(v.begin(), v.end());
  int n = 0;
  for (auto& s : v) { if (names) { if (n >= max) break; snprintf(names[n], SD_NAME_LEN, "%s", s.c_str()); } n++; }
  return n;
}
int count(const char* dir, const char* ext) { return list(dir, ext, nullptr, 0); }
char* load(const char* path, size_t maxLen) {
  if (!sdMounted) return nullptr;
  FILE* f = fopen((sdRoot + path).c_str(), "rb");
  if (!f) return nullptr;
  fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
  if (n <= 0 || (size_t)n > maxLen) { fclose(f); return nullptr; }
  char* b = (char*)malloc(n + 1);
  if (fread(b, 1, n, f) != (size_t)n) { fclose(f); free(b); return nullptr; }
  fclose(f); b[n] = 0; return b;
}
}

static void shot(const std::string& name) {
  std::string p = "tools/sim/out/" + name + ".ppm";
  FILE* f = fopen(p.c_str(), "wb");
  if (!f) { perror(p.c_str()); return; }
  fprintf(f, "P6 128 128 255\n");
  for (int i = 0; i < 128 * 128; i++) {
    uint16_t c = (uint16_t)((fb[i] >> 8) | (fb[i] << 8));
    unsigned char px[3] = { (unsigned char)(((c >> 11) & 31) << 3), (unsigned char)(((c >> 5) & 63) << 2), (unsigned char)((c & 31) << 3) };
    fwrite(px, 1, 3, f);
  }
  fclose(f);
}

static void runFrames(int n) {
  for (int i = 0; i < n; i++) {
    simUs += 33333;
    auto t0 = std::chrono::steady_clock::now();
    app::frame();
    double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    frameCpuMs += ms;
    if (ms > maxCpuMs) maxCpuMs = ms;
  }
}

int main(int argc, char** argv) {
  srand(1234);
  if (getenv("SIM_SD")) sdRoot = getenv("SIM_SD");
  if (const char* pre = getenv("SIM_SAVE")) {   // "anahtar=deger,anahtar=deger" ile kayıt ön yükleme
    char* s = strdup(pre);
    for (char* kv = strtok(s, ","); kv; kv = strtok(nullptr, ",")) {
      char* eq = strchr(kv, '=');
      if (eq) { *eq = 0; store[kv] = atoi(eq + 1); }
    }
    free(s);
  }
  mkdir("tools/sim/out", 0755);
  app::init();
  int total = 0;
  for (int a = 1; a < argc; a++) {
    char* script = strdup(argv[a]);
    for (char* tok = strtok(script, " \n"); tok; tok = strtok(nullptr, " \n")) {
      if (!strncmp(tok, "shot:", 5)) { shot(tok + 5); continue; }
      char* star = strchr(tok, '*');
      int n = star ? atoi(star + 1) : 1;
      if (star) *star = 0;
      simButtons = 0;
      for (char* c = tok; *c; c++) {
        switch (*c) {
          case 'F': simButtons |= 1 << 0; break; case 'U': simButtons |= 1 << 1; break;
          case 'L': simButtons |= 1 << 2; break; case 'R': simButtons |= 1 << 3; break;
          case 'D': simButtons |= 1 << 4; break; case 'X': simButtons |= 1 << 5; break;
        }
      }
      runFrames(n);
      total += n;
    }
    free(script);
  }
  printf("[sim] %d kare, ort %.3f ms, en fazla %.3f ms (host CPU)\n", total, total ? frameCpuMs / total : 0, maxCpuMs);
  return 0;
}
