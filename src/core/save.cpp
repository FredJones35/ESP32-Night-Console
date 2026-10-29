#include "save.h"
#include <Preferences.h>
#include <stdio.h>

namespace save {

static Preferences prefs;
static bool opened;

void init() {
  opened = prefs.begin("konsol", false);
}

int32_t get(const char* key, int32_t def) {
  if (!opened || !prefs.isKey(key)) return def;
  return prefs.getInt(key, def);
}

void set(const char* key, int32_t value) {
  if (!opened) return;
  if (prefs.isKey(key) && prefs.getInt(key, value + 1) == value) return;
  prefs.putInt(key, value);
}

bool setMax(const char* key, int32_t value) {
  if (!opened) return false;
  if (prefs.isKey(key) && prefs.getInt(key, 0) >= value) return false;
  prefs.putInt(key, value);
  return true;
}

bool setMin(const char* key, int32_t value) {
  if (!opened) return false;
  if (prefs.isKey(key) && prefs.getInt(key, 0) <= value) return false;
  prefs.putInt(key, value);
  return true;
}

void reset() {
  if (opened) prefs.clear();
}

void keyFor(char* out, const char* prefix, const char* name) {
  uint32_t h = 2166136261u;   // FNV-1a
  for (const char* p = name; *p; p++) { h ^= (uint8_t)*p; h *= 16777619u; }
  snprintf(out, 16, "%.6s%08x", prefix, (unsigned)h);
}

}  // namespace save
