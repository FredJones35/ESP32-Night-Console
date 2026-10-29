// Açılış ekranı, oyun listesi ve Ayarlar
#pragma once

namespace menu {
void init();          // açılışta bir kez (açılış ekranıyla başlar)
void enter();         // oyundan dönüşte (skorları yeniler)
int  update(float dt);  // seçilen oyunun indeksi ya da -1
void render();
}
