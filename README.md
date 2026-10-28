# ESP32 DOOM

ESP32 DevKit V1 ve 1.44" 128x128 renkli TFT ekran üzerinde çalışan, sıfırdan yazılmış **Doom tarzı bir raycaster** oyunu. Oyun 5 butonla oynanıyor, harici kütüphane olarak yalnızca [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) kullanılıyor.

> Bu proje id Software'in Doom'unun bir portu değil. Bütün grafikler (dokular, düşman, silah) kodun içinde prosedürel olarak üretiliyor, hiçbir WAD dosyası veya orijinal oyun verisi kullanılmıyor.

## Özellikler

- Wolfenstein/Doom tarzı raycasting motoru, ~80 FPS (240 MHz)
- 3 farklı prosedürel duvar dokusu (tuğla, metal panel, taş) ve uzaklığa göre gölgelendirme
- Z-buffer ile duvar arkasında gizlenen sprite düşmanlar
- Oyuncuyu görünce takip edip yakın dövüşte saldıran 8 düşman
- Yürürken sallanan silah, namlu alevi ve hasar yiyince kırmızı kenarlık
- HUD: can, mermi, öldürme sayısı
- Başlık, ölüm ve bölüm sonu ekranları

## Donanım

| Parça | Adet |
|---|---|
| ESP32 DevKit V1 (ESP32-WROOM-32) | 1 |
| 1.44" 128x128 SPI TFT ekran (ST7735) | 1 |
| Tactile buton | 5 |
| Breadboard ve jumper kablolar | — |

## Bağlantı Şeması

![Bağlantı şeması](docs/wiring.svg)

### Ekran

| Ekran pini | ESP32 |
|---|---|
| SDA (MOSI) | D13 |
| SCK / SCL | D14 |
| CS | D15 |
| A0 / DC | D27 |
| RESET | D26 |
| VCC | 3V3 |
| LED / BL | 3V3 |
| GND | GND |

> Ekranın üzerinde SDA/SCL yazıyor ama bu bir **SPI** ekranı. I2C pinlerine (D21/D22) **bağlanmamalı**.

### Butonlar

Her butonun bir ucu ESP32 pinine, diğer ucu GND'ye bağlanıyor. Kodda dahili pull-up kullanıldığı için harici direnç gerekmiyor.

| Buton | ESP32 |
|---|---|
| Ateş et | D19 |
| İleri git | D4 |
| Sola bak | D32 |
| Sağa bak | D25 |
| Geri git | D33 |

> **Not:** 4 bacaklı butonlarda bacaklar ikişer ikişer içeriden birbirine bağlı. Kabloları **çapraz** bacaklara takın, yoksa buton sürekli basılı okunur.

## Kurulum

[PlatformIO](https://platformio.org/) gerekiyor. Debian/Ubuntu için:

```bash
# PlatformIO
sudo apt install python3-venv
curl -fsSL -o get-platformio.py https://raw.githubusercontent.com/platformio/platformio-core-installer/master/get-platformio.py
python3 get-platformio.py
echo 'export PATH="$PATH:$HOME/.platformio/penv/bin"' >> ~/.bashrc && source ~/.bashrc

# Seri port izni (sonra oturumu kapatıp açın)
sudo usermod -aG dialout $USER
```

Derleme ve yükleme:

```bash
git clone https://github.com/<kullanici-adi>/esp32doom.git
cd esp32doom
pio run -t upload        # ya da ./flash-game.sh
pio device monitor       # FPS çıktısı
```

TFT_eSPI'nin bütün ayarları `platformio.ini` içindeki `build_flags` ile veriliyor. Bu yüzden kütüphanenin `User_Setup.h` dosyasını düzenlemeye gerek yok.

## Kontroller

| Buton | Görev |
|---|---|
| Ateş et | Ateş et (basılı tutunca otomatik ateş) / oyunu başlat / yeniden başla |
| İleri / Geri | Yürü |
| Sola / Sağa bak | Dön |

Bütün düşmanları öldürdüğünüzde bölüm tamamlanıyor. Öldürülen her düşman +4 mermi veriyor.

## Sorun Giderme

| Belirti | Çözüm |
|---|---|
| Ekran tamamen beyaz | Kablolama hatalı. SDA→D13, SCK→D14 bağlantılarını kontrol edin. Seri monitörde `FPS:` görünüyorsa kod çalışıyor demektir. |
| Görüntü 32 piksel kaymış ya da üstte karlı şerit var | `platformio.ini` içindeki sürücü ayarını değiştirin: `ST7735_GREENTAB3` → `GREENTAB128` / `GREENTAB2` / `BLACKTAB` |
| Kenarda 1–2 piksellik renkli çizgi var | `ST7735_GREENTAB2` veya `ST7735_BLACKTAB` deneyin |
| Kırmızı ile mavi yer değiştirmiş | `-D TFT_RGB_ORDER=TFT_BGR` satırını açın |
| Görüntü bozuk ya da karlı | `SPI_FREQUENCY` değerini `20000000` veya `10000000` yapın |
| Ateş butonu basılmadan sürekli ateş ediyor | Buton bacakları yanlış, kabloları çapraz bacaklara takın. `TFT_MISO=-1` ayarının da yerinde olduğundan emin olun (D19 normalde VSPI MISO pini). |
| Yükleme `Unable to verify flash chip connection` hatası veriyor | D12'ye ya da CMD/SD0–SD3/CLK pinlerine kablo bağlanmış olabilir. Bu pinler boş kalmalı. |

## Proje Yapısı

```
esp32doom/
├── platformio.ini     # kart, kütüphane ve ekran ayarları
├── flash-game.sh      # derle + yükle kısayolu
├── src/
│   └── main.cpp       # oyun: raycaster, düşmanlar, HUD, girişler
└── docs/
    └── wiring.svg     # bağlantı şeması
```

## Nasıl Çalışıyor?

- **Render:** Ekranın her sütunu için oyuncunun bakış yönünde bir ışın gönderiliyor. Işın DDA algoritmasıyla 16x16'lık harita ızgarasında ilerliyor. Duvara çarptığı mesafeye göre sütun yüksekliği ve doku koordinatı hesaplanıyor.
- **Framebuffer:** 128x128 RGB565 görüntü (32 KB) RAM'de bir `TFT_eSprite` içinde çiziliyor ve her karede tek seferde SPI ile ekrana gönderiliyor.
- **Sprite'lar:** Düşmanlar kamera uzayına dönüştürülüyor, uzaktan yakına sıralanıyor ve sütun bazında z-buffer'a göre kırpılarak çiziliyor.
- **Dokular:** Açılışta hash tabanlı gürültüyle 32x32 boyutunda üretiliyor, flash'ta resim dosyası tutulmuyor.
