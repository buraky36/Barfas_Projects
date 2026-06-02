# Walkthrough - BLE Smart Gateway Firmware

Bu dokümanda, **ESP32-C6** tabanlı modüler Gateway yazılımı (FAZ 1) için gerçekleştirilen çalışmalar, oluşturulan kod yapısı ve derleme adımları özetlenmektedir.

---

## 1. Gerçekleştirilen Çalışmalar ve Kod Yapısı

`Firmware/` klasörü altında, ESP-IDF v5.2+ standartlarına uygun modüler bir FreeRTOS projesi kurulmuştur. Oluşturulan bileşenler (components) ve işlevleri şunlardır:

### 📁 Proje Klasör Yapısı
* [Firmware/CMakeLists.txt](file:///c:/Users/hp/Desktop/Barfas_Projects/Barfas_Projects/myProjects/2_Smart_Gateway/Firmware/CMakeLists.txt): Projenin kök CMake dosyası.
* [Firmware/platformio.ini](file:///c:/Users/hp/Desktop/Barfas_Projects/Barfas_Projects/myProjects/2_Smart_Gateway/Firmware/platformio.ini): PlatformIO konfigürasyon dosyası.
* [Firmware/sdkconfig.defaults](file:///c:/Users/hp/Desktop/Barfas_Projects/Barfas_Projects/myProjects/2_Smart_Gateway/Firmware/sdkconfig.defaults): BLE yığınını (**NimBLE**) ve hedef çipi (**ESP32-C6**) aktifleştiren varsayılan konfigürasyon.
* [Firmware/main/CMakeLists.txt](file:///c:/Users/hp/Desktop/Barfas_Projects/Barfas_Projects/myProjects/2_Smart_Gateway/Firmware/main/CMakeLists.txt) & [main.c](file:///c:/Users/hp/Desktop/Barfas_Projects/Barfas_Projects/myProjects/2_Smart_Gateway/Firmware/main/main.c): Sistem orkestrasyonu, LED'lerin durum kontrolü ve factory reset (fiziksel buton) mekanizması.

### 🧩 Oluşturulan Bileşenler (Components)
1. **[config_manager](file:///c:/Users/hp/Desktop/Barfas_Projects/Barfas_Projects/myProjects/2_Smart_Gateway/Firmware/components/config_manager)**:
   * NVS Flash bellek üzerinden Wi-Fi ve MQTT ayarlarını güvenli bir şekilde kaydeder ve okur.
2. **[wifi_manager](file:///c:/Users/hp/Desktop/Barfas_Projects/Barfas_Projects/myProjects/2_Smart_Gateway/Firmware/components/wifi_manager)**:
   * Wi-Fi STA (bağlanma) ve AP (kurulum) modlarını asenkron olarak yönetir. Bağlantı kopmalarında otomatik yeniden bağlanma döngüsünü çalıştırır.
3. **[web_server](file:///c:/Users/hp/Desktop/Barfas_Projects/Barfas_Projects/myProjects/2_Smart_Gateway/Firmware/components/web_server)**:
   * AP modunda yerel ağdan (`192.168.4.1`) erişilebilen şık bir **glassmorphic dark-mode** yapılandırma arayüzü sunar. Girilen parametreleri kaydederek cihazı otomatik yeniden başlatır.
4. **[ble_manager](file:///c:/Users/hp/Desktop/Barfas_Projects/Barfas_Projects/myProjects/2_Smart_Gateway/Firmware/components/ble_manager)**:
   * NimBLE yığınını başlatır, çevreyi tarayıp **Onloi Beacon** verilerini süzerek MQTT'ye aktarır. Ayrıca sunucudan gelen komutları alıp ilgili kilide GATT bağlantısı kurarak yazar.
5. **[mqtt_manager](file:///c:/Users/hp/Desktop/Barfas_Projects/Barfas_Projects/myProjects/2_Smart_Gateway/Firmware/components/mqtt_manager)**:
   * TLS destekli asenkron MQTT bağlantısını yönetir. BLE tarama sonuçlarını JSON formatında `/gateway/{device_id}/scan_report` konusuna publish eder ve `/gateway/{device_id}/command` konusunu dinler.

---

## 2. Derleme ve Yükleme Adımları

Projenizi derleyip yüklemek için aşağıdaki üç yöntemden birini tercih edebilirsiniz:

### Seçenek A: PlatformIO Kullanarak (Tavsiye Edilen)
1. VS Code'a **PlatformIO IDE** eklentisini kurun.
2. VS Code'da **File -> Open Folder** menüsünden doğrudan `Firmware/` klasörünü açın.
3. PlatformIO projeyi otomatik olarak tanıyacak ve bağımlılıkları çekecektir.
4. Alt durum çubuğundaki kısayolları kullanabilirsiniz:
   * **Derleme (Build):** `✓` (Checkmark simgesi)
   * **Yükleme (Upload/Flash):** `→` (Arrow simgesi)
   * **Seri Monitör:** `🔌` veya fiş simgesi (115200 baud rate ile otomatik açılır)

### Seçenek B: VS Code ESP-IDF Eklentisi Kullanarak
1. VS Code'da `Firmware/` klasörünü açın.
2. Eklenti menüsünden hedef çipi **ESP32-C6** (`esp32c6`) olarak seçin.
3. Sol alt durum çubuğundaki **Build Project** (silindir) ve **Flash Device** (şimşek) simgelerini kullanın.

### Seçenek C: ESP-IDF Terminali Kullanarak
1. Windows Başlat menüsünden **ESP-IDF 5.2 PowerShell** terminalini açın.
2. Proje dizinine gidin:
   ```powershell
   cd C:\Users\hp\Desktop\Barfas_Projects\Barfas_Projects\myProjects\2_Smart_Gateway\Firmware
   ```
3. Hedefi ayarlayın ve derleyin:
   ```powershell
   idf.py set-target esp32c6
   idf.py build
   ```
4. Kodu yükleyin ve seri ekranı izleyin:
   ```powershell
   idf.py -p COMx flash monitor
   ```

---

## 3. Seri Port Debug ve Sistem Log İzleme

Proje genelinde hata tespitini kolaylaştırmak için detaylı log etiketleri tanımlanmıştır:
* `[CORE_MGR]`: Sistem açılış, bellek ve LED adımlarını gösterir.
* `[WIFI_MGR]`: Wi-Fi AP/STA bağlantı olaylarını gösterir.
* `[CONFIG_MGR]`: NVS okuma ve yazma işlemlerini doğrular.
* `[WEB_SERVER]`: Gelen HTTP isteklerini ve kaydedilen form verilerini gösterir.
* `[BLE_MGR]`: Bulunan Onloi beacon paketlerini (MAC, RSSI, veri) ve kilit bağlantı adımlarını gösterir.
* `[MQTT_MGR]`: Broker bağlantı durumlarını ve publish/subscribe işlemlerini gösterir.
