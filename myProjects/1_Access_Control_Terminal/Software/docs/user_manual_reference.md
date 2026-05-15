# Akıllı Geçiş Kontrol Terminali - Kullanım Kılavuzu Referans Belgesi

Bu doküman, cihazın özellikleri, çalışma/geçiş modları, programlama adımları ve ağ (Online/Offline) kuralları hakkında User Manual (Kullanıcı Kılavuzu) hazırlarken faydalanabileceğiniz temel teknik ve operasyonel bilgileri içermektedir.

## 1. Donanım ve Temel Özellikler

- **İşlemci Mimarisi:** ESP32 tabanlı mikrodenetleyici.
- **Kullanıcı Arayüzü:** Çok renkli RGB LED durum göstergesi, Sesli uyarı (Buzzer) ve Kapasitif Dokunmatik Tuş Takımı (TSM12/XR1300).
- **Okuyucu Donanımları:**
  - 13.56MHz RFID Okuyucu (MFRC522)
  - Barkod ve QR Kod Okuyucu Modülü
- **Bağlantı ve İletişim:**
  - Çift yönlü Wiegand 26/34-bit haberleşme (Giriş ve Çıkış).
  - Wi-Fi ve BLE (Bluetooth Low Energy) — Mobil uygulama üzerinden ilk kurulum (provisioning) için kullanılır.
  - **Not:** Cihaz Wiegand Reader (Mod 3) olarak ayarlandığında Wi-Fi ve BLE radyoları tamamen devre dışı bırakılır; cihaz kablosuz ağlarda görünmez olur.
- **Dijital Girişler (Digital Inputs):**
  - **Digital Input 1 (GPIO 19) → Exit Button (Kapı Açma Butonu):** Kapıyı içeriden açmak için kullanılan çıkış butonu. Aktif HIGH, kenar-tetiklemeli (basış anında bir kez tetiklenir). Pull-down dirençli.
  - **Digital Input 2 (GPIO 21) → Door Sensor (Kapı Durum Sensörü):** Kapının fiziksel açık/kapalı durumunu izler. Aktif HIGH, seviye-tetiklemeli (kapı açık kaldığı sürece sürekli aktif). Pull-down dirençli, 5 ardışık örnekle debounce edilir.
- **Çıkışlar:**
  - Röle (Shift Register üzerinden sürülür) — Kapı kilidi veya elektrikli kol kontrolü için.
- **Bellek (NVS Storage):** Çevrimdışı (Offline) çalışma durumunda Master Kod, Kullanıcı ID'leri, PIN'ler ve Kart verilerinin kalıcı olarak saklandığı güvenli yerel hafıza.

---

## 2. Çalışma Modları (Working Modes - Menü 6)

Cihaz kurulum esnasında bulunduğu fiziksel konuma ve sisteme göre 3 farklı rolde çalıştırılabilir:

1. **Mod 1 - Standalone (Bağımsız Mod):** 
   Cihaz dışarıdan hiçbir okuyucuya bağlı değildir. Tüm geçiş kararları kendi belleğindeki kullanıcılara göre verilir. Bu moddayken cihazın Wiegand girişinden gelen veriler (harici bir okuyucu takılsa bile) güvenlik amacıyla yok sayılır.
   
2. **Mod 2 - Controller (Denetleyici Modu):** 
   Cihaz hem kendi üzerindeki okuyucuları (Tuş Takımı, RFID vb.) kullanır hem de dışarıdan (Wiegand üzerinden) bağlanan harici bir okuyucudan gelen verileri dinler. Geçiş kararlarını kendisi verir ve kendi rölesini tetikler.

3. **Mod 3 - Reader (Sadece Okuyucu Modu):** 
   Cihaz hiçbir geçiş kararı vermez ve rölesini çekmez. Cihaza okutulan kart, şifre veya QR kod verileri doğrudan Wiegand Çıkışı üzerinden arka plandaki ana panele/denetleyiciye iletilir. Cihaz ayarlarına ulaşmak için Master Menüye erişim (Tuş takımı ile) bu modda dahi açıktır. 
   **Not:** Güvenlik ve enerji verimliliği nedeniyle Mod 3'e geçildiğinde cihaz kendini yeniden başlatır ve Wi-Fi ile BLE radyoları tamamen kapalı (görünmez) olarak çalışır.

---

## 3. Geçiş İzin Modları (Access Modes - Menü 5)

Kullanıcıların hangi yöntemleri kullanarak geçiş yapabileceği belirlenebilir:
- **Desteklenen Seçenekler:** Sadece Kart, Sadece PIN, Sadece QR, Kart veya PIN, Kart veya QR, veya Herhangi Biri.
- **Hatalı Girişim:** Cihaz örneğin "Sadece Kart" modundayken bir kullanıcı şifre (PIN) girmeye çalışırsa, işlem doğrudan reddedilir ve cihaz **"3 Kısa Bip + Sürekli Kırmızı LED"** ile uyarı verir.

---

## 4. Programlama Modu ve Cihaz Ayarları

Cihazın tüm yapılandırması tuş takımı üzerinden yapılabilir.

- **Programlama Moduna Giriş:** 
  Bekleme (IDLE) ekranında `*` tuşuna basılır, ardından `[Master Kod]` ve `#` tuşlanarak menüye girilir.
- **Programlama Modundan Çıkış:**
  Menü içerisinde herhangi bir adımdayken `*` tuşuna basıldığında tüm işlemler anında iptal edilir ve doğrudan cihaz bekleme moduna döner.
- **Menü 2 - Kullanıcı Ekleme:**
  Kullanıcı eklerken yapısal sıralama şöyledir: `<Menü 2> -> <Kullanıcı ID> -> # -> <PIN Şifresi veya Kart Okutma> -> #`
- **Menü 3 - Kullanıcı Silme:**
  Spesifik bir ID girilerek sadece o kullanıcı silinebilir. Eğer silme adımında **Master Kod** girilirse, güvenlik teyidi ile birlikte **tüm kullanıcılar sistemden kalıcı olarak silinir**.
- **Menü 4 - Röle Süresi (Toggle Modu):**
  Kapı açılış süresi belirlenir. Eğer süre **`0` (Sıfır)** olarak kaydedilirse cihaz **Toggle (Aç/Kapat)** moduna geçer. Yani yetkili biri geçiş yaptığında kapı sürekli açık kalır; bir sonraki yetkili geçişte ise kapı kapanır.
- **Menü 7 - Açık Kapı Tespiti (Door Detection):**
  Kapının açık kalma süresi limitleri belirlenir. `0` ile `3` dakika arasında ayarlanabilir, ayarlanan süre aşılırsa cihaz alarm durumuna geçer.

---

## 5. Operasyonel Ayrıntılar ve Ağ Kuralları (ÖNEMLİ)

### Online ve Offline Karar Mekanizması
- **İlk Bağlantı ve Online Mod:** Cihaz kutudan ilk çıktığında çevrimdışı (Offline) çalışır. Ancak bir kez internete (Wi-Fi) bağlandığında ve sunucuyla iletişim kurduğunda, kendisini kalıcı olarak **Online Mod**'a kilitler.
- **Offline Çalışmaya Dönüş İmkansızlığı:** Online moda geçen bir cihazın sunucuyla bağlantısı kesilse bile (Wi-Fi kopsa dahi) cihaz "geçici" olarak kapıyı açmaz (Offline yerel belleğine bakmaz). **Online olmuş bir cihazın bir daha Offline sistemle çalışabilmesi için cihazın mutlaka Fabrika Ayarlarına döndürülmesi zorunludur.**
- **Sunucu Otoritesi:** Online moddayken okutulan tüm kart, şifre ve QR verileri (HTTP/REST veya MQTT üzerinden) doğrudan sunucuya iletilir; geçiş kararı sadece sunucudan gelen API yanıtına göre uygulanır.

### QR Kod Doğrulaması Sadece Online Modda Çalışır
- QR kodlar genellikle dinamik, anlık süreli ve yüksek karakterli şifreli veriler (hash) taşıdıkları için donanımın yerel hafızasında saklanması mimari olarak uygun değildir. Bu nedenle **QR Okuma yöntemi sadece cihazın Online Modda olduğu ve sunucu ile başarılı bir iletişim kurabildiği durumlarda geçerlidir.**

### Kullanıcı Tarafından Şifre (PIN) Değiştirme
Kullanıcılar cihazı programlama moduna sokmadan, direkt bekleme ekranında (IDLE) şifrelerini değiştirebilirler:
1. `*` tuşuna basılır.
2. `[Kullanıcı ID]` girilir ve `#` tuşuna basılır. *(Cihaz girilen kodun Master Kod olmadığını anlar ve Şifre Değiştirme adımına geçer).*
3. Önce **Eski Şifre**, ardından **Yeni Şifre** ve tekrar **Yeni Şifre** girilerek işlem tamamlanır. Her adım `#` tuşuyla onaylanır.
