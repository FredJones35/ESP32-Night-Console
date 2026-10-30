# Donanım Test Raporu — ESP32 Gece Konsolu

Tarih: 30.10.2026

## Test ortamı

| | |
|---|---|
| Kart | ESP32 DevKit V1 (ESP32-WROOM-32, PSRAM yok), USB-seri CP2102 |
| Ekran | 1.44" ST7735, HSPI, `ST7735_GREENTAB3`, `TFT_MISO=-1`, `setRotation(0)`, SPI 27 MHz |
| Butonlar | 6 buton, `INPUT_PULLUP`, pinler README'deki gibi |
| SD kart | Testin başında takılıydı ama boştu. Kartsız test için çıkarıldı. Sonra `sd/` içeriği (9 dosya) kartın köküne kopyalandı ve testin geri kalanı kart takılıyken yapıldı. Kart ~31 GB, FAT. |
| Test edilen commit | Başlangıç `34083f5`; düzeltmelerden sonra son test `0aae731` (temiz derleme, 0 uyarı) |
| Derleme | `pio run` uyarısız. RAM 23 384 B (%7.1), flash 483 189 B (%15.4). |

Ölçümler seri logdan alındı (saniyede bir `FPS` satırı, 115200 baud). Test sırasında girişe geçici bir buton logu ve en sonda Doom bölümlerini açan geçici bir satır eklendi. İkisi de commit'lenmedi, son firmware'de yok.

## FPS ve bellek tablosu

Oyun oynanırken ölçüldü. Heap değerleri bayt cinsinden, SD kart takılıyken. Kart takılıyken SD sürücüsü yaklaşık 27 KB ayırır.

| Oyun / bölüm | Ort. FPS | En düşük FPS | Oyunda boş heap | Oyunun ayırdığı | En büyük blok |
|---|---|---|---|---|---|
| DOOM E1M1 Giriş Kapısı | 68 | 45 | 245 856 | 35 400 | 110 580 |
| DOOM E1M2 Kanlı Laboratuvar | 66 | **39** | 245 856 | 35 400 | 110 580 |
| DOOM E1M3 Atık Tünelleri (SD) | 67 | 52 | 245 856 | 35 400 | 110 580 |
| DOOM E1M4 Et Mabedi (SD) | 59 | 44 | 245 856 | 35 400 | 110 580 |
| DOOM E1M5 Kasabın İni (SD, boss) | 65 | 49 | 245 856 | 35 400 | 110 580 |
| YARIŞ Şeytan Virajları | 75 | 57 | 266 508 | 14 748 | 110 580 |
| YARIŞ Çöl Fırtınası | 76 | 61 | 266 508 | 14 748 | 110 580 |
| MOTOKROS Isınma Turu | 67 | 59 | 274 096 | 7 160 | 110 580 |
| MOTOKROS Kum Tepeleri | 64 | 59 | 273 832 | 7 160 | 110 580 |
| YILAN | 90 | 84 | 279 700 | 1 292 | 110 580 |
| BLOKLAR | 89 | 84 | 280 692 | 300 | 110 580 |
| FLAPPY | 88 | 83 | 280 748 | 244 | 110 580 |
| Menü | 82–87 | — | 281 256 (SD'siz 308 736) | — | 110 580 |

- **Doom ve Yarış hedefi (≥30 FPS) karşılandı.** En kötü an E1M2'de yakın düşmanlar varken 39 FPS. Hasar alınırken (kırmızı ekran) de ölçüldü.
- Test boyunca görülen en düşük boş heap 220 108 bayttı (yarış pisti SD'den yüklenirken kısa süreli). En büyük blok hiç değişmedi.
- FPS 30'un altına inmediği için zamanlama ölçümüne ve optimizasyona (DMA, 40 MHz vb.) gerek kalmadı.

### Giriş/çıkış ve sızıntı

Her oyunda menüye dönüşte (`Oyundan cikildi ... heap`) değer aynıya döndü:

| Oyun | Giriş/çıkış sayısı | Çıkıştaki heap |
|---|---|---|
| DOOM | 10 | 281 256 (SD'siz 308 736), her seferinde aynı |
| YARIŞ | 4 | 281 256, her seferinde aynı |
| MOTOKROS | 4 | 280 992, her seferinde aynı (aşağıdaki nota bakın) |
| YILAN / BLOKLAR / FLAPPY | 1 / 2 / 1 | 280 992 |

Not: Açılıştan sonraki ilk Motokros oturumunda 264 B kalıcı olarak düşüyor (281 256 → 280 992). Bu bir sızıntı değil. Motokros logunda ondalıklı `printf` (`dustu x=%.1f`) var, newlib de ilk ondalıklı yazdırmada bir kez tampon ayırıp sonra tekrar kullanıyor. Sonraki her girişte değer 280 992'de sabit kaldı.

## Test listesi

### 1. Açılış ve ekran

| Madde | Sonuç | Not |
|---|---|---|
| "GECE KONSOLU" açılış ekranı hemen geliyor | Geçti | |
| Renkler doğru (kırmızı/mavi ters değil) | Geçti | |
| Kayma, kenarda karlanma ya da renkli çizgi yok | Geçti | `GREENTAB3` doğru |
| SD takılıyken ve takılı değilken ekran çalışıyor (HSPI/VSPI ayrımı) | Geçti | |
| Tuşa basınca oyun listesine geçiliyor | Geçti | |

### 2. Butonlar

| Madde | Sonuç | Not |
|---|---|---|
| 6 butonun her biri doğru tepki veriyor | Geçti | Logda her basış ayrı bir bas/bırak çifti olarak geldi |
| ATEŞ kendi kendine tetiklenmiyor (D19) | Geçti | Saatlerce süren logda kendiliğinden buton olayı yok |
| Debounce, bir basış iki kez sayılmıyor | Geçti | Menüde hızlı basışlarda çift sayım görülmedi. Not: ~2700 basışın 12'sinde bırakma ile yeni basış arası 28–45 ms. Bu, eski breadboard butonlarında sekme olabilir. Çift sayım fark edilirse `input.cpp`'deki `DEBOUNCE_MS` 25'ten 40'a çıkarılabilir. |

### 3. Menü ve Ayarlar

| Madde | Sonuç | Not |
|---|---|---|
| 6 oyun ve Ayarlar listede, ikonlar ve skor satırları görünüyor, kaydırma yumuşak | Geçti | |
| SD durumu ve SD satırına basınca yeniden tarama | Geçti | Takma, çıkarma ve yeniden takma doğru algılandı |
| Doom/Yarış/Moto dosya sayısı | Geçti | Boş kartta 0/0/0, dosyalar kopyalanınca 3/3/3 |
| FPS göstergesi açılıp kapanıyor, sağ üstte sayı var | Geçti | Güç kesilince ayar korunuyor |
| Kayıt sıfırlama: GERİ iptal, ATEŞ siliyor | Geçti | |

### 4. SD kart

| Madde | Sonuç | Not |
|---|---|---|
| Kart açılışta tanınıyor | **Kaldı → düzeltildi** | 20 MHz'de `f_mount failed: (1)`. 4 MHz ile düzeldi (commit `a48e1b9`) |
| Kartsız: "SD kart: YOK", 2/2/2 bölüm, "SD yok: 2 ..." | Geçti | |
| Kartlı: Ayarlar 3/3/3, seçim ekranlarında 5 giriş, SD etiketi | Geçti | |
| Karttan bir bölüm yüklenip oynanıyor (E1M3) | Geçti | E1M3, E1M4, E1M5, Yarış ve Moto 3. parkurları karttan yüklendi |

### 5. Performans ve bellek

| Madde | Sonuç | Not |
|---|---|---|
| Doom ≥30 FPS (yakın düşman ve hasar alırken dahil) | Geçti | En düşük 39 |
| Yarış ≥30 FPS | Geçti | En düşük 57 |
| Diğer oyunlar rahat | Geçti | 59–90 FPS |
| Her oyuna 3 kez gir/çık, heap aynıya dönüyor | Geçti | Yılan ve Flappy 1, Bloklar 2 kez girildi. Bunlar en az bellek ayıran oyunlar ve her çıkışta değer aynı. |

### 6. Oyunlar

| Madde | Sonuç | Not |
|---|---|---|
| Her oyunda Kontroller ekranı | Geçti | |
| SOLA+SAĞA 1.5 sn: çubuk, onay kutusu, GERİ oyuna, ATEŞ menüye | Geçti | |
| **Doom:** bölüm seçimi, kilitli bölüm seçilemiyor | Geçti | "KILITLI" uyarısı |
| Doom: yürüme, dönme, ateş | Geçti | |
| Doom: otomatik reload, reload sırasında ateş yok | Geçti | |
| Doom: RELOAD tek basış elle reload, çift basış silah değiştirme | Geçti | |
| Doom: kapılar, anahtar uyarısı, "GIZLI ODA!", çıkış | Geçti | |
| Doom: istatistik ekranından sonraki bölüme geçiş | Geçti | E1M1 → E1M2 oynanarak |
| Doom: ölünce ATEŞ ile yeniden başlama | Geçti | |
| Doom: E1M5 boss | Geçti | E1M3–E1M5 geçici olarak açılarak test edildi (kullanıcı E1M2'yi geçemedi). Bölüm dosyalarına dokunulmadı. |
| Doom: karanlık, sis, titreyen ışık, hasarda kırmızı efekt | Geçti | |
| **Yılan:** hızlanma, altın bonus yem, çarpınca bitiş, rekor | Geçti | |
| **Bloklar:** hareket/tekrar, döndürme, GERİ yavaş, İLERİ anında düşürme, RELOAD bekletme, satır efekti, seviye | Geçti | |
| **Flappy:** zıplama, çarpışma, 10 puandan sonra hareketli sütunlar, rekor | Geçti | |
| **Yarış:** geri sayım, gaz/fren/direksiyon, savrulma, yol dışı, nitro, rakipler, sıra, tur süresi | Geçti | |
| Yarış: sonuç ekranı, ilk 3'e girince yeni pist açılıyor | Geçti | 5 pistin hepsi oynanarak açıldı. O oturumun seri logu bozuk çıktı yüzünden okunamadı (aşağıya bakın), sonuç kullanıcının ekranda gördüğüne dayanıyor. |
| **Motokros:** gaz/fren, eğilme, takla | **Kaldı → düzeltildi** | Geri taklalar sayılmıyordu. Commit `0aae731`, donanımda doğrulandı. |
| Motokros: ters inişte düşme, kontrol noktasından devam | Geçti | |
| Motokros: RELOAD ile kontrol noktasına dönme | Geçti | |
| Motokros: madalya, bronz ve üstü sonraki parkuru açıyor | Geçti | Isınma Turu gümüş → Kum Tepeleri açıldı |

### 7. Kalıcı kayıt

| Madde | Sonuç | Not |
|---|---|---|
| Rekorlar ve ilerleme kaydediliyor | Geçti | |
| Güç tamamen kesilip açılınca korunuyor | Geçti | USB çıkarıldı, 5 sn beklendi |
| Ayarlar'dan sıfırlayınca hepsi siliniyor | Geçti | FPS göstergesi de kapandı |

## Bulunan hatalar ve çözümleri

| # | Hata | Neden | Çözüm | Commit |
|---|---|---|---|---|
| 1 | SD kart takılı olduğu hâlde "SD kart: YOK" | Kart 400 kHz'deki ilk el sıkışmaya cevap veriyor ama 20 MHz'de ilk sektör okuması başarısız oluyor (`f_mount failed: (1) A hard error occurred in the low level disk I/O layer`). Uzun breadboard kabloları ve modüldeki seviye çevirici bu hız için yetersiz. | SD saat hızı 4 MHz'e düşürüldü. Bölüm dosyaları en fazla 16 KB, yükleme süresine etkisi birkaç on ms. | `a48e1b9` |
| 2 | Motokros'ta geri taklalar "TAKLA!" vermiyor | Dönüş yalnızca iki teker havadayken toplanıyor ve ilk teker yere değince değerlendiriliyordu. Rampadan burun yukarı kalkılıp arka tekere inilince tam bir geri takla ~360° − rampa açısı ölçülüyor ve 306°'lik eşiğin altında kalıyordu. Öne takla ise 360° + rampa açısı ölçüldüğü için sayılıyordu. | Dönüş iki tekerin yerden kesildiği andan ikisinin tekrar yere bastığı ana kadar ölçülüp tam tura yuvarlanıyor. Başta ve sonda motor zemine paralel olduğu için iki yön de doğru sayılıyor. | `0aae731` |
| — | README: ölçülen FPS ve yeni sorun giderme satırları | | | `0f4cdb8` |

## Çözülemeyen ya da kullanıcı kararı gereken konular

1. **Bir kez açıklanamayan reset (20:19).** Doom oynarken kart yeniden başladı ve aynı anda USB-seri bağlantı da koptu. CP2102 de gittiği için yazılım çökmesinden çok güç ya da kablo kesintisi gibi duruyor. Tekrarlanmadı ve logda çökme (Guru Meditation) izi yok. Tekrarlarsa USB kablosu/portu ve SD modülünün 5 V beslemesi kontrol edilmeli. Brownout olup olmadığını görmek için açılış logundaki `rst:` satırına bakılmalı.
2. **USB yeniden takılınca seri log bozuluyor.** Kart USB çıkarılıp takıldıktan ya da yüklemeden sonra (bir kez 20:47'de USB kendiliğinden yeniden bağlandığında) seri çıktı hiçbir baud hızında okunamadı. Oyun çalışmaya devam etti, EN/reset sonrası log temizlendi. Firmware'i etkilemiyor, README'ye sorun giderme satırı olarak eklendi. Ara sıra satır arasında birkaç bozuk bayt da görüldü (gürültü). Bu da oyunu etkilemiyor.
3. **Debounce sınırda.** Yukarıdaki not: çok az sayıda 28–45 ms'lik bırak/bas aralığı var. Kullanıcı çift sayım görmediği için değiştirilmedi. Görülürse `DEBOUNCE_MS = 40` önerilir.
4. **Boş SD klasörleri için gürültülü log.** Kartta `/doom` vb. klasörler yoksa her taramada ESP32 SD kütüphanesi `open(): /sd/doom does not exist` hata satırı yazıyor. Zararsız, istenirse `sd::list` içinde `SD.exists()` kontrolüyle susturulabilir.
5. **Doom zorluğu.** Kullanıcı E1M2'yi geçmekte zorlandı. Kullanıcının isteğiyle bölümler ve zorluk değiştirilmedi.
6. **Telefonda biçimlendirme.** Kart telefonla doldurulduysa ve 32 GB'tan büyükse telefon onu exFAT biçimlendirebilir. ESP32 SD kütüphanesi exFAT okuyamaz. README'ye not eklendi.
