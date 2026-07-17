# Protokol Karşılaştırması ve Analiz Raporu

## 1. Genel Bakış ve Temel Fark
Bizim hazırladığımız (eski) protokol, ağ geçidinin kilitlerle **şifresiz (Plaintext) ve doğrudan** konuştuğu, gelen hex komutlarını olduğu gibi ilettiği basit ve hızlı bir "şeffaf köprü" (transparent bridge) modeliydi.

Mobil/Sunucu yazılımcısının gönderdiği yeni `v1.0` protokolü ise **uçtan uca şifrelemeli (End-to-End Encryption)** ve **Yerel Anahtar (Local Key)** mimarisine dayanan, çok daha kompleks ve üst düzey güvenlikli bir yapıya geçmiş. BLE (Bluetooth) sadece bir taşıyıcı olarak kullanılıyor, asıl veri AES-GCM ile şifreleniyor.

---

## 2. Neler Değişmiş / Eklenmiş?

### 🛡️ Güvenlik ve Şifreleme (En Büyük Değişiklik)
- **Eskiden:** Sunucu `/command` üzerinden `010203...` gönderir, biz doğrudan kilide yazardık. Kilit açılırdı.
- **Yeni:** Artık hiçbir komut düz metin (plaintext) gitmiyor. Ağ geçidi veya mobil uygulama bir kilide bağlanmak istediğinde önce geçici anahtarlar üretip (ECDH X25519) `SESSION_INIT (0x70)` adında bir "el sıkışma" işlemi yapmak zorunda. Komutlar AES-256-GCM ile şifreleniyor.

### 🔌 Ağ Geçidi İlk Kurulumu (Onboarding)
- **Eskiden:** Ağ geçidinin Wi-Fi ağına nasıl bağlanacağı (ilk kurulum) protokolde tanımlı değildi.
- **Yeni:** Ağ geçidi ilk açıldığında kendisi bir BLE cihazı gibi yayın (Advertise) yapıyor. Kullanıcı telefonla ağ geçidine bağlanıp `KEX_INIT` ile şifreli bir tünel kuruyor ve Wi-Fi şifresini ağ geçidine BLE üzerinden güvenli şekilde iletiyor.

### 🔗 Cihaz Ekleme (Claiming) Mekanizması
- **Eskiden:** Ağ geçidi etrafı tarar (`/scan_report`), sunucu dilediği MAC adresine komut gönderirdi.
- **Yeni:** Cihazlar sahiplenilebilir (Claimed) veya sahipsiz (Unclaimed) olarak ikiye ayrılmış. Ağ geçidi sahipsiz bir kilit gördüğünde sunucu `/device_claim` komutu gönderiyor. Ağ geçidi kilide bağlanıp ona özel bir kriptografik anahtar (`localKey`) üreterek içine enjekte ediyor (`KEY_DELIVER`).

### 📡 MQTT Topic'lerindeki Değişiklikler
- **Yeni Eklenenler:** `/device_claim`, `/device_unclaim`, `/key_rotate`, `/sub_ota` gibi kilit yönetimine dair yeni dinleme (subscribe) topic'leri eklenmiş.
- **`/scan_report` Değişimi:** Artık sadece MAC ve RSSI yetmiyor. Ağ geçidinin kilidin yayınladığı (Advertise) veriyi parçalayıp kilit modelini (`deviceCode`: örn OK0310) ve sahiplik durumunu (`claimed`: true/false) sunucuya bildirmesi isteniyor.
- **`/notify` Değişimi:** Kilit kendi kendine bir bildirim gönderdiğinde (örn: kapı açıldı), ağ geçidinin bunu doğrudan sunucuya iletmesi yetmiyor; bu mesaj şifreli olduğu için sunucunun çözebilmesi adına ağ geçidinin o oturuma ait `nonce` ve `public key` bilgilerini de JSON içine eklemesi gerekiyor.

### 🔄 OTA (Uzaktan Güncelleme) Güvenliği
- **Eskiden:** Sunucu sadece `{"url": "http..."}` gönderiyordu, biz indirip kuruyorduk.
- **Yeni:** Sunucu artık `sha256` ve `signature` (Ed25519 İmza) gönderiyor. Ağ geçidinin dosyayı indirdikten sonra kurmadan önce bu imzayı kendi içindeki açık anahtar ile doğrulaması isteniyor (Kötü niyetli yazılım yüklenmesini engellemek için).

---

## 3. Bizim Yazılımımız Şu Anda Neleri Yapabiliyor?

Şu anki `v1.2.0` yazılımımız donanımsal olarak çok güçlü bir altyapıya sahip, ancak yeni protokolün getirdiği kriptografik (şifreleme) iş mantığı henüz kodumuzda yok. 

**Şu an yapabildiklerimiz (Yeni protokole uyanlar):**
1. **Blind Relay (Kör İletim):** Yeni protokolde sunucu şifreli hex stringini `/command` üzerinden gönderiyor ve ağ geçidinin içini okumadan doğrudan BLE'ye yazmasını istiyor. Bizim yazılımımız **zaten tam olarak bunu yapıyor.** Komut iletme altyapımız yeni sistemle %100 uyumlu.
2. **Scan Report:** BLE tarama ve cihazları bulup MQTT'ye basma (`/scan_report`) işlemimiz kusursuz çalışıyor. Sadece gelen byte'ların içinden `deviceCode` kısmını ayıklayıp JSON'a yeni bir değişken olarak eklememiz gerekecek.
3. **OTA İndirme ve Flaşlama:** HTTP/HTTPS üzerinden .bin dosyasını indirip hafızaya yazma işlemimiz mükemmel çalışıyor. Sadece öncesinde imza doğrulama mantığı (Ed25519) eklenecek.
4. **Bağlantı ve Kararlılık:** MQTT LWT (Last Will), Wi-Fi kopma/bağlanma, RGB LED göstergeleri ve bellek yönetimimiz yeni protokolün beklentilerini tamamen karşılıyor.

**Yapamadıklarımız (Eklenmesi Gerekenler):**
1. Ağ geçidinin kendisinin bir BLE cihazı gibi davranıp telefondan Wi-Fi şifresi alma özelliği (BLE Peripheral Mode).
2. Kilitlerle bağlantı kurulurken yapılan `SESSION_INIT` (ECDH X25519 el sıkışması) kriptografik güvenlik adımları.
3. Yeni eklenen MQTT topic'lerinin (`/device_claim` vb.) dinlenmesi ve kilitlere `KEY_DELIVER` yapılması.

**Özetle:** Biz altyapıyı (otoyolu) mükemmel bir şekilde inşa ettik. Sunucu yazılımcısı ise bu otoyoldan geçen araçların "zırhlı" olmasını ve ehliyet kontrolü yapılmasını istemiş. Yazılımımızın temel mimarisi sağlam olduğu için bu şifreleme ve yeni topic gereksinimleri üzerine kolayca inşa edilebilir.
