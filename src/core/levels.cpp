#include "levels.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hw.h"
#include "parse.h"

void levelTitle(const char* text, char* out, int n, const char* fallback) {
  snprintf(out, n, "%s", fallback);
  TextReader r(text);
  while (r.next()) {
    if (r.section[0]) break;
    const char *k, *v;
    if (r.kv(&k, &v) && streq(k, "name")) { snprintf(out, n, "%s", v); break; }
  }
}

void LevelSet::build(const char* const* builtins, int nb, const char* dir, const char* ext) {
  count = 0;
  for (int i = 0; i < nb && count < MAX; i++, count++) {
    builtin[count] = builtins[i];
    path[count][0] = 0;
    char dflt[16];
    snprintf(dflt, sizeof(dflt), "#%d", count + 1);
    levelTitle(builtins[i], title[count], sizeof(title[0]), dflt);
  }
  builtinCount = count;
  if (!sd::ok()) return;

  char (*names)[SD_NAME_LEN] = (char (*)[SD_NAME_LEN])malloc(MAX * SD_NAME_LEN);
  if (!names) return;
  int n = sd::list(dir, ext, names, MAX - count);
  for (int i = 0; i < n && count < MAX; i++) {
    snprintf(path[count], sizeof(path[0]), "%s/%s", dir, names[i]);
    char* txt = sd::load(path[count]);
    if (!txt) { hwLog("SD okunamadi: %s", path[count]); continue; }
    char fbName[20];
    snprintf(fbName, sizeof(fbName), "%.*s", (int)(strlen(names[i]) - strlen(ext)), names[i]);
    levelTitle(txt, title[count], sizeof(title[0]), fbName);
    free(txt);
    builtin[count] = nullptr;
    count++;
  }
  free(names);
}

const char* LevelSet::load(int i, bool* owned) const {
  *owned = false;
  if (i < 0 || i >= count) return nullptr;
  if (builtin[i]) return builtin[i];
  char* t = sd::load(path[i]);
  *owned = t != nullptr;
  return t;
}

void LevelSet::release(const char* text, bool owned) const {
  if (owned) free((void*)text);
}
