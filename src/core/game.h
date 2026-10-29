// Ortak oyun arayüzü. Her oyun kendi namespace'i içinde bir Game nesnesi tanımlar.
//   enter : bellek burada alınır (false dönerse menüye geri dönülür)
//   update: oyun mantığı, dt saniye
//   render: framebuffer'a tam kare çizim (pushSprite'ı uygulama yapar)
//   exit  : enter'da alınan her şey burada bırakılır
#pragma once
#include <stdint.h>

struct Game {
  const char* name;                          // menüde görünen ad
  void (*icon)(int x, int y, float t);       // 24x24 ikon
  void (*scoreText)(char* buf, int n);       // menüdeki en iyi skor / ilerleme satırı
  const char* const* controls;               // "BUTON", "eylem" çiftleri, nullptr ile biter
  bool (*enter)();
  void (*update)(float dt);
  void (*render)();
  void (*exit)();
};

extern const Game* const GAMES[];
extern const int GAME_COUNT;

namespace app {
void  init();
void  frame();
void  requestMenu();   // oyun kendisi menüye dönmek isterse
bool  showFps();
void  setShowFps(bool on);
float time();          // açılıştan beri saniye
}
