# Akıllı Geçiş Kontrol Terminali İş Planı

Bu doküman, ESP32/ESP-IDF altyapısı üzerine inşa edilen Smart Access Control Terminal V1.0 projesi için baştan uca iş paketlerini (Work Breakdown Structure - WBS) ve tahmini sürelerini (Adam/Gün) içermektedir.

> [!NOTE]  
> Çıkarılan süreler, gömülü yazılım mühendisliği pratikleri çerçevesinde test, hata ayıklama (debug) ve kenar vaka (edge case) analizleri dâhil edilerek "mantıklı/gerçekçi" çerçevede planlanmıştır.

---

## 📦 Paket 1: Donanım Sürücüleri (HAL) ve Entegrasyon
**Tahmini Süre:** `3 - 4 Gün`
**Durum:** *(Büyük Ölçüde Tamamlandı)*

Projenin elektronik bileşenleriyle sorunsuz konuşması için gerekli olan ve platform bağımsız hale getirilmiş (Hardware Abstraction Layer) katmanının ayağa kaldırılmasıdır.
- **`hal_rfid.c` ve `mfrc522.c`**: SPI bus yapılandırması, 13.56MHz okuma sürekliliği ve çakışma (conflict) yönetimi.
- **`hal_shift_reg.c`**: 74HC595 üzerinden kapasitif klavyenin arkasındaki Çok Renkli LED (RGB) ışıkları ve Buzzer için Non-Blocking (Bloklamayan) animasyon motoru oluşturulması.
- **`hal_touch.c`**: TSM12/XR1300 dokunmatik entegrelerin I2C ile debounce edilerek hatasız tuş okuması.
- **`hal_wiegand.c`**: Çift yönlü Wiegand altyapısı (Format 26/34 bit girişi ve çıkışı).
- **`hal_qr.c`**: Barkod/QR okuyucu modül seri haberleşmesi (UART/RS232).

---

## 📦 Paket 2: Core İş Mantığı (State Machine) ve Yerel Veri
**Tahmini Süre:** `4 - 6 Gün`
**Durum:** *(Tamamlandı / Test Aşamasında)*

Güvenlik sisteminin çevrimdışı (offline) beyni olan çekirdek işleyiş kurallarının (Onloi Document bazlı) tasarlanıp kodlanması işlemleri. *`app_state_machine.c` içerisinde uyguladığımız kısımlardır.*
- **Durum Makinesi Tasarımı (State Machine):** `IDLE`, `PROGRAMMING`, `DOOR_OPEN`, `ALARM`, `PIN_CHANGE` geçişleri.
- **Programlama Menüsü (Master Menu):** `*` ve `#` algoritması ile Cihaz Modu, Röle Süresi, Wiegand Tipi, Access Modları, Ses ve Işık ayarlarının konfigüre edilmesi.
- **Ziyaretçi Kalıcı Belleği (NVS Storage):** Master Code, Kullanıcı PIN, Kart/QR ID'lerinin Flash üzerindeki ayrılmış bir partition'a JSON formatında veya struct blokları halinde güvenli okunup yazılması.
- **Standalone, Controller, Reader Modları:** Wiegand'ın okuyucu ya da denetleyici olmasına karar verilen izolasyon katmanı.
- **Yetki Kuralları:** Card Only, Card+PIN, QR+PIN kombinasyonlu erişim kontrolleri.

---

## 📦 Paket 3: Ağ Yönetimi (Wi-Fi/BLE) ve Bulut Entegrasyonu
**Tahmini Süre:** `5 - 7 Gün`
**Durum:** *(Sırada / Başlanılacak)*

Donanımı ayağa kalkan ve yerelde stabil çalışan cihazın, bulut mimarisiyle entegre edilmesi.
- **BLE Provisioning (Bluetooth Kurulum):** Cihazda ekran olmadığı için, cep telefonu veya mobil uygulama ile cihaza Bluetooth (BLE) üzerinden bağlanılarak Wi-Fi SSID ve Şifresinin cihaza aktarılması.
- **Wi-Fi Heartbeat (Ağ Yönetimi):** Sürekli internet bağlantısının kopma/bağlanma durumlarının asenkron takibi.
- **Online Geçiş Kuralı:** Cihaz ilk internete çıktığında kalıcı olarak sisteme `config.is_online = true` kaydetmesi ve yerel (Offline) kararlarını ezip, yalnızca Sunucudan gelen (API) kararlara uyması.
- **HTTP/REST API İstemcisi:** Karta okutulan her bilginin (örn. `{"type":"RFID", "data":"123456"}`) Onloi veya müşteri API'lerine HTTPS Client üzerinden iletilip, `200 OK` veya `403 Forbidden` yanıtının işlenmesi.

---

## 📦 Paket 4: Over-The-Air (OTA) Güncelleme Sistemi
**Tahmini Süre:** `2 - 3 Gün`

Sahada var olan veya müşterinin duvarına takılmış bir cihaza, yeni bir versiyonu uzaktan yüklemek için gerekli alt yapının konfigürasyonu.
- **OTA Partition Ayarlaması:** Cihazın `partitions.csv` dosyasının fabrika ve 2 x OTA (A/B) olarak ayarlanması.
- **API Üzerinden Update Çekme:** Cihazın gece saatlerinde (veya uzaktan gelen bir komutla) sunucuyu kontrol edip yeni `.bin` yazılım dosyasını indirerek kendi üzerine güvenle (Rollback destekli) yazması ve yeniden başlaması.

---

## 📦 Paket 5: Sistem Stres Testleri, Optimizasyon ve Kalite (QA)
**Tahmini Süre:** `3 - 5 Gün`

Prototipten ürüne geçen süreçteki en kritik adım olan köşe (edge case) sorunlarının yakalanması.
- **Memory Analizi:** Cihaz günlerce çalıştığında State Machine yapısında hafıza (Heap Leak) şişmesi olmaması için RAM takibi.
- **Ağ Kopma Testleri:** Cihaz okutma işlemi sırasında API isteğini atıp Wi-Fi koptuğunda, sürecin tıkanmadan 3-5 saniye timeout a düşerek lokal bekleme durumuna dönüp dönmediği.
- **Elektriksel Noise Debug:** Kapı rölesi çekerken endüktif bir yük olduğu için Wiegand veya Tuş Takımı sensörlerine parazit ile hatalı tuş basma/Reset attırma durumlarının gözlenmesi.

---

### Mimarî Özet ve Takvim

| Aşama No | İş Paketi Kategorisi | Tahmini Efor (Gün) |
| --- | --- | --- |
| 1 | Donanım ve Sürücüler (HAL) | 3 - 4 G |
| 2 | Çevrimdışı Çalışma Mantığı (State Machine) | 4 - 6 G |
| 3 | Wi-Fi / Uygulama ve Sunucu (API) Haberleşmesi | 5 - 7 G |
| 4 | Buluttan Uzaktan Güncelleme (OTA) | 2 - 3 G |
| 5 | Edge-Case Testleri ve Kalite (QA) Onayı | 3 - 5 G |
| **TOPLAM** | **Geliştirme, Entegrasyon, Test ve Canlıya Alış** | **~17 - 25 İş Günü (3.5 - 5 Hafta)** |

> [!TIP]
> Şu ana kadar yaptığımız kodlama ve Refactoring sayesinde *Paket 1* ve *Paket 2*'yi oldukça geride bıraktık. Ana çatısını bitirdik ve test aşamasındayız. Bu nedenle aslında projenin **%40'lık lokal gömülü yazılım (offline logic)** tarafı hazır diyebiliriz. Kalan efor daha çok bulut/API haberleşmesine ve uç vakaların (edge cases) test edilmesine harcanacaktır.
