# Implementation Plan - BLE Smart Gateway Firmware (ESP32-C6)

Bu plan, ESP32-C6 üzerinde çalışacak, FreeRTOS tabanlı modüler Gateway yazılımının (FAZ 1) oluşturulmasını ve modüllerinin kodlanmasını kapsar.

---

## 1. Proje Yapısı ve Dizin Düzeni

Yazılım projesi, en güncel **ESP-IDF v5.2+** standartlarına uygun, CMake tabanlı ve bileşen odaklı (component-based) modüler bir yapıda kurulacaktır. Proje `Firmware/` dizini altında oluşturulacaktır.

```
Firmware/
├── CMakeLists.txt                  # Ana CMake dosyası
├── sdkconfig.defaults              # Varsayılan target ve bileşen ayarları (ESP32-C6, NimBLE vb.)
├── main/
│   ├── CMakeLists.txt
│   └── main.c                      # Sistem orkestratörü (Core Manager)
└── components/
    ├── config_manager/             # NVS ayarları okuma/yazma
    │   ├── CMakeLists.txt
    │   ├── config_manager.c
    │   └── include/config_manager.h
    ├── wifi_manager/               # Wi-Fi AP/STA bağlantı yönetimi
    │   ├── CMakeLists.txt
    │   ├── wifi_manager.c
    │   └── include/wifi_manager.h
    ├── ble_manager/                # BLE 5.3 Scanner/GATT Client (NimBLE)
    │   ├── CMakeLists.txt
    │   ├── ble_manager.c
    │   └── include/ble_manager.h
    ├── mqtt_manager/               # TLS MQTT Bağlantısı ve Veri Aktarımı
    │   ├── CMakeLists.txt
    │   ├── mqtt_manager.c
    │   └── include/mqtt_manager.h
    └── web_server/                 # Konfigürasyon Web GUI (HTTP Server)
        ├── CMakeLists.txt
        ├── web_server.c
        └── include/web_server.h
```

---

## 2. Modüler Bileşenlerin Detayları

Her bir modül, kendi FreeRTOS Task'ı veya event handler'ı üzerinde çalışarak diğerlerinden bağımsız (loosely coupled) olacaktır.

### A. Config Manager (NVS)
* **İşlev:** Wi-Fi SSID, Wi-Fi Şifre, MQTT Sunucu Adresi, Port, Client ID ve Cihaz ID bilgilerini Non-Volatile Storage (NVS) üzerinde depolar.
* **Log Etiketi:** `[CONFIG_MGR]`
* **Debug Çıktıları:** Ayarlar okunduğunda veya kaydedildiğinde başarı/hata durumları detaylıca konsola yazdırılır.

### B. Wi-Fi Manager
* **İşlev:** STA (İnternet bağlantısı) ve AP (Cihaz kurulumu için yerel ağ) modlarını yönetir.
* **Log Etiketi:** `[WIFI_MGR]`
* **Debug Çıktıları:** IP adresi alımı, kopma durumları, AP modu aktifleşme durumları net mesajlarla loglanır.

### C. Web Server
* **İşlev:** AP modunda yerel ağdan (`192.168.4.1`) erişilebilen hafif bir web arayüzü sunar.
* **Log Etiketi:** `[WEB_SERVER]`
* **Debug Çıktıları:** Gelen HTTP istekleri (URI, method), alınan POST parametreleri ve kaydetme durumları loglanır.

### D. BLE Manager (NimBLE Stack)
* **İşlev:** NimBLE Bluetooth yığınını başlatır.
  * **Scanner:** Çevredeki BLE cihazlarını ve Onloi Beacon'larını dinler, verileri yakalayıp JSON'a dönüştürür ve MQTT modülüne iletir.
  * **GATT Client:** Sunucudan gelen kilit açma/kapama komutlarını aldığında ilgili kilit UUID'sine bağlanıp GATT yazma işlemi gerçekleştirir.
* **Log Etiketi:** `[BLE_MGR]`
* **Debug Çıktıları:** Tarama esnasında keşfedilen Onloi Beacon paket detayları (RSSI, MAC, Raw Data), GATT bağlantı kurma, servis keşfi ve veri yazma başarı/hata durumları detaylıca basılır.

### E. MQTT Manager
* **İşlev:** ESP-MQTT kütüphanesini kullanarak uzak sunucuya SSL/TLS şifreli bağlanır.
* **Log Etiketi:** `[MQTT_MGR]`
* **Debug Çıktıları:** Bağlantı denemeleri, başarılı bağlantı (`CONNECTED`), topic abonelik durumları (`SUBSCRIBED`), gönderilen veriler (payload boyutu ve topic) ve alınan komutlar konsola net şekilde yazdırılır.

### F. Main (Core Orchestrator)
* **İşlev:** Sistem başlatıldığında tüm modülleri sırasıyla ayağa kaldırır, aralarındaki FreeRTOS kuyruk ve event gruplarını oluşturur.
* **Log Etiketi:** `[CORE_MGR]`
* **Debug Çıktıları:** Sistem açılış sırası, serbest bellek durumu (heap) ve LED bildirim durumları loglanır.

---

## 3. Doğrulama ve Test Planı

1. **Bileşenlerin Simülasyonu:** `idf.py` ile proje derlenerek derleme hataları kontrol edilecektir.
2. **Wi-Fi AP & Web GUI Testi:** ESP32-C6'nın AP modunda ayağa kalkması ve mobil/bilgisayar üzerinden bağlanıp Web GUI arayüzünün görünmesi test edilecektir.
3. **BLE Scanning Testi:** Yakındaki BLE kilit ve beacon verilerinin taranıp seri port ekranında (LOG) doğru çözümlendiği doğrulanacaktır.
4. **FreeRTOS Kararlılık Testi:** Bellek sızıntıları (heap leak) ve task stack durumları `esp_get_free_heap_size()` ile izlenecektir.
