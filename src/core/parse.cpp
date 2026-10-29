#include "parse.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

bool streq(const char* a, const char* b) {
  while (*a && *b) {
    if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return false;
    a++; b++;
  }
  return *a == *b;
}

bool TextReader::next() {
  while (*p) {
    const char* s = p;
    while (*p && *p != '\n') p++;
    const char* e = p;
    if (*p) p++;
    while (s < e && (*s == ' ' || *s == '\t')) s++;
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r')) e--;
    if (s == e || *s == '#') continue;
    int n = (int)(e - s);
    if (n >= (int)sizeof(line)) n = sizeof(line) - 1;
    if (*s == '[') {
      const char* q = s + 1;
      int k = 0;
      while (q < e && *q != ']' && k < (int)sizeof(section) - 1) section[k++] = *q++;
      section[k] = 0;
      continue;
    }
    memcpy(line, s, n);
    line[n] = 0;
    return true;
  }
  return false;
}

bool TextReader::in(const char* sec) const { return streq(section, sec); }

bool TextReader::kv(const char** key, const char** val) {
  char* eq = strchr(line, '=');
  if (!eq) return false;
  char* k = line;
  char* ke = eq;
  while (ke > k && (ke[-1] == ' ' || ke[-1] == '\t')) ke--;
  *ke = 0;
  char* v = eq + 1;
  while (*v == ' ' || *v == '\t') v++;
  *key = k;
  *val = v;
  return true;
}

int parseInts(const char* s, int* out, int max) {
  int n = 0;
  while (*s && n < max) {
    while (*s && !(isdigit((unsigned char)*s) || *s == '-' || *s == '+')) s++;
    if (!*s) break;
    char* end;
    long v = strtol(s, &end, 10);
    if (end == s) { s++; continue; }
    out[n++] = (int)v;
    s = end;
  }
  return n;
}

int parseFloats(const char* s, float* out, int max) {
  int n = 0;
  while (*s && n < max) {
    while (*s && !(isdigit((unsigned char)*s) || *s == '-' || *s == '+' || *s == '.')) s++;
    if (!*s) break;
    char* end;
    float v = strtof(s, &end);
    if (end == s) { s++; continue; }
    out[n++] = v;
    s = end;
  }
  return n;
}

int toInt(const char* s, int def) {
  int v;
  return parseInts(s, &v, 1) == 1 ? v : def;
}

float toFloat(const char* s, float def) {
  float v;
  return parseFloats(s, &v, 1) == 1 ? v : def;
}

const char* splitWord(char* s, char** rest) {
  while (*s == ' ' || *s == '\t') s++;
  char* w = s;
  while (*s && *s != ' ' && *s != '\t') s++;
  if (*s) *s++ = 0;
  while (*s == ' ' || *s == '\t') s++;
  *rest = s;
  return w;
}
