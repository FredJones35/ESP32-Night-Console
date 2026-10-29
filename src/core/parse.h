// Basit metin dosyası okuyucu (bölüm/pist dosyaları için).
//   # ile başlayan satırlar yorumdur, boş satırlar atlanır.
//   [bolum] satırları bölüm değiştirir; ilk bölümden önceki satırlar başlıktır (key=value).
#pragma once
#include <stdint.h>

struct TextReader {
  explicit TextReader(const char* text) : p(text) { section[0] = 0; line[0] = 0; }

  bool next();                        // sıradaki anlamlı satıra geç
  bool in(const char* sec) const;     // geçerli bölüm adı (büyük/küçük harf duyarsız)
  bool kv(const char** key, const char** val);  // "key=value" ise satırı ikiye böler

  char section[16];
  char line[128];

 private:
  const char* p;
};

int   parseInts(const char* s, int* out, int max);      // boşluk/virgül ayrılmış
int   parseFloats(const char* s, float* out, int max);
bool  streq(const char* a, const char* b);              // büyük/küçük harf duyarsız
int   toInt(const char* s, int def = 0);
float toFloat(const char* s, float def = 0);
// "word rest..." biçiminde ilk kelimeyi ayırır; rest boşluklardan arındırılmış döner
const char* splitWord(char* s, char** rest);
