// Bölüm/pist listesi: firmware'e gömülü metinler + SD karttaki dosyalar.
// Gömülü bölümler SD dosyalarıyla aynı metin formatındadır, aynı ayrıştırıcıdan geçer.
#pragma once
#include "sd.h"

struct LevelSet {
  static const int MAX = 16;
  int  count;
  int  builtinCount;
  char title[MAX][20];
  const char* builtin[MAX];      // gömülü metin (SD girdilerinde nullptr)
  char path[MAX][40];            // SD yolu

  // dir: "/doom", ext: ".lvl". Başlıklar dosyadaki name= satırından okunur.
  void build(const char* const* builtins, int nb, const char* dir, const char* ext);
  // Metni döndürür. owned=true ise çağıran free() ile bırakmalıdır. Hata: nullptr.
  const char* load(int i, bool* owned) const;
  void release(const char* text, bool owned) const;
};

// Metnin başlığından name= değerini okur
void levelTitle(const char* text, char* out, int n, const char* fallback);
