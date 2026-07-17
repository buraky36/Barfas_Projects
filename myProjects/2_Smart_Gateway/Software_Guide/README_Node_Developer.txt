Onloi Node Cihazi Gelistirici Rehberi
-------------------------------------

Merhaba, 
Bu klasordeki yazilim paketleri, Gateway projesinden ayrilarak Node (Kilit) cihazinizin gelistirmesini hizlandirmak amaciyla ozel olarak paketlenmistir. 

Paket Icerigi:

1. ble_frame_parser/
Bu modul Onloi v1.0 BLE Frame Standartina gore byte dizilimini yaplandiran saf C dili ile yazilmis fonksyionlari icerir. Donanim bagimsizdir.
- `ble_frame_parser.h` ve `.c` dosyalarini kendi projenize ekleyebilirsiniz.
- Paketleri (SOF 0x4F ile baslayan) parse etmek ve cevap (Build Response) uretmek icin kullanilir.

2. crypto_manager/
Bu modul mbedtls tabanli olup X25519 (ECDH) ortak anahtar turetimi, HKDF ve AES-256-GCM sifreleme algoritmalarini icerir. 
- ESP32 kullaniyorsaniz `crypto_manager.c` dosyasini dogrudan kendi bilesenlerinize (components) ekleyebilirsiniz.
- Eger farkli bir islemci kullaniyorsaniz, sadece "esp_fill_random()" olan rastgele sayi uretim fonksiyonunu kendi islemcinize gore degistirerek geri kalan tum mbedtls mantigini koruyabilirsiniz.

Not: Gateway yazilimimiz bir "Transparent Bridge" oldugu icin kilit payload verilerini decrypt etmez. Ancak sizin gelistireceginiz Node cihazi bir "Endpoint" oldugu icin kendisine gelen payload verilerini bu `crypto_manager` altindaki `crypto_aes_gcm_decrypt` fonksiyonu ile acmali ve cevaplari `crypto_aes_gcm_encrypt` ile sifrelemelidir.


