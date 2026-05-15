#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <Preferences.h>
#include "esp_bt.h"
#include <sys/time.h>

// Sunucu Ayarları
//#define WEB_URL "https://onloi.universeio.com/nowhere"
#define WEB_URL "https://api.onloi.com/api/device/runtime"

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// Cihaz Kimlik Bilgileri
#define device_id   "OnloiLock2025"

#define Header  0x55AA
#define Version 0x00
#define Wifi_Status_Data_Len 1

  //  ##  MCU   ##  //
#define Wifi_Reset1    0x03   // Wifi Sıfırla
#define Wifi_Reset2    0x04   // Wifi Sıfırla + Seçilebilir Eşleştirme Modu
#define Log_State      0x05   // Log Durumu bildieimi (Keypad + RFID + Fingerprint : ADMIN+USER)
#define GMT_Time       0x10   // Saat güncelleme isteği
#define Temporary_Pass 0xDD   // Geçici Parola isteği
  //  ##  MCU   ##  //
  //  ##  TUYA   ##  //
#define Product_Info   0x01   //ürün bilgisini sorgulama
#define Wifi_Status    0x02   // Wifi durum sorgulama(Sürekli gönderim)
#define Module_Reset   0x25   // Modul sıfırlama durum bildirme
  //  ##  TUYA   ##  //

//----- Haberleşme -----//
#define RX_PIN 16
#define TX_PIN 17
#define BAUD_RATE 115200
#define BUFFER_SIZE 256
#define STREAM_SIZE  256

HardwareSerial SerialUART(1); // UART1 kullanılıyor

String response;
bool Write_Data_Flag = false;
bool ble_Connecting = false;

bool Remote_Open = false;
unsigned long remoteOpenTimestamp = 0;
const unsigned long REMOTE_OPEN_LOCK_MS = 15000;  // 10 saniye

//uint8_t buffer[BUFFER_SIZE];
static uint8_t  stream_buf[STREAM_SIZE];
static size_t   stream_len = 0;

volatile bool commandInProgress = false;

//----- Haberleşme -----//

BLECharacteristic *pCharacteristic;
Preferences preferences;
String incomingBuffer = "";
unsigned long lastReceiveTime = 0;
const unsigned long CHUNK_TIMEOUT_MS = 3000;
String storedDeviceKey = "";
String storedPairKey = "";
bool wifiConnected = false;

BLEServer* pServer = NULL;
bool deviceConnected = false;

bool open_request = false;
bool open_password_request = false;

// Root CA Sertifikası (Geliştirme için geçici olarak devre dışı)
const char* rootCACertificate = R"EOF(
-----BEGIN CERTIFICATE-----
(SUNUCUNUZUN SSL SERTİFIKASINI BURAYA EKLEYİN)
-----END CERTIFICATE-----
)EOF";

WiFiClientSecure client;
HTTPClient http;

// Durum bildirim periyodu
const unsigned long STATUS_PERIOD = 3000;
unsigned long lastStatusTime = 0;

// Zaman için global değişkenler
time_t               serverEpoch  = 0;
unsigned long long   serverMillis = 0;

bool shouldStopBLE = false;

bool bleMode = false;

// Komut türleri
enum CommandType {
  LOG,
  PASS,
  STATE,
  OPEN
};

// Log türleri
enum LogType {
  FINGERPRINT1,
  KEYPAD1,
  RFID1
};

typedef struct {
    uint16_t header;    // sabit 0x55AA
    uint8_t  version;
    uint8_t  command;
    uint16_t length;    // big-endian
    uint8_t* data;      // malloc ile ayrılacak
    uint8_t  checksum;
} Frame;

// Prototipler
void sendCommand(CommandType cmdType, LogType logType = RFID1, String passValue = "");
void handleResponse(int httpCode, const String& payload);
void resetWiFi();
void sendChunkedData(String data);
void sendData(String data);
bool connectToWiFi(const char* ssid, const char* password);
bool loadWiFiCredentials(String &ssid, String &pass);
void saveWiFiCredentials(const char* ssid, const char* pass);
void clearWiFiCredentials();
void saveDeviceKeys(const char* deviceKey, const char* pairKey);
bool loadDeviceKeys(String &deviceKey, String &pairKey);
void clearDeviceKeys();
void factoryReset();
void startBLE();

bool parseNextFrame(Frame* out);
void processIncoming(const uint8_t* buf, size_t len);
uint8_t getWifiStateCode();

void sendStatusUART(uint8_t status);
void sendNetworkStatus(uint8_t statusCode);
void sendOpenRequestFrame();

void setup() {
  Serial.begin(115200);
  delay(100);
  WiFi.mode(WIFI_STA);
//  WiFi.disconnect(true);

  // RX pinine yazılımsal pull-up ekle (boştaysa gürültüyü engeller)
  pinMode(RX_PIN, INPUT_PULLUP);
  // UART1 başlat
  SerialUART.begin(BAUD_RATE, SERIAL_8N1, RX_PIN, TX_PIN);
  Serial.println("UART echo hazır. TX=17, RX=16, Baud=115200");
  
  if(loadDeviceKeys(storedDeviceKey,storedPairKey)) {
    Serial.print("DeviceKey (flash): ");
    Serial.println(storedDeviceKey);
    Serial.print("PairKey (flash): ");
    Serial.println(storedPairKey);
  }else{
    Serial.println("Kayıtlı Devicekey ve PairKey bilgisi bulunamadı.");
  }
  
  String savedSsid, savedPass;
  if (loadWiFiCredentials(savedSsid, savedPass)) {
    Serial.print("Ssid:");
    Serial.println(savedSsid);
    Serial.print("Pass:");
    Serial.println(savedPass);

    if (connectToWiFi(savedSsid.c_str(), savedPass.c_str())) {
      wifiConnected = true;
      Serial.println("WiFi bağlantısı başarılı.");

      // SSL sertifikası
      client.setCACert(rootCACertificate);
      client.setInsecure(); // Sertifika doğrulamasını devre dışı bırak (Geliştirme için)

      // HTTP zaman aşımıf
      http.setTimeout(15000);

      if (getServerTime()) {
       Serial.println("⏰ Sunucudan saat başarıyla alındı...");
      } else {
       Serial.println("⏰ Sunucu saati alınamadı...");
      }
      return;
    } else {
      Serial.println("WiFi bağlantısı başarısız.");
      return;
    } 
  } else {
    Serial.println("Kayıtlı WiFi bilgisi bulunamadı.");
    Serial.println("BLE bağlantısı başlatılıyor...");
    startBLE();
    Serial.println("Karakteristik tanımlandı! Telefonunuzdan erişebilirsiniz.");
  }
}

void loop() {

  // Timeout kontrolü: uzun süre veri gelmediyse buffer temizlenir
/*  if (incomingBuffer.length() > 0 && (millis() - lastReceiveTime > CHUNK_TIMEOUT_MS)) {
    Serial.println("⏱️ Timeout: Veri eksik kaldı, tampon temizleniyor.");
    Serial.print("📦 Gelen veri (eksik): ");
    Serial.println(incomingBuffer);
    incomingBuffer = "";
  }*/

  if(Write_Data_Flag)
  {
    sendChunkedData(response);
    Write_Data_Flag = false;
    if(shouldStopBLE)
      ble_Connecting = true;
  }
    

  unsigned long now = millis();

    // 1) Remote_Open süresini kontrol et
  if (Remote_Open && (now - remoteOpenTimestamp >= REMOTE_OPEN_LOCK_MS)) {
    Remote_Open = false;
    Serial.println("🔓 Remote_Open süresi doldu, tekrar açmaya izin veriliyor.");
  }

  // 2) Periyodik durum raporu
  if (!bleMode && (now - lastStatusTime >= STATUS_PERIOD)) {
    lastStatusTime = now;
    // 1) Durum kodunu al
    uint8_t code = getWifiStateCode();
    Serial.printf("Wifi State : 0x%02X\n", code);
    // 2) Frame'i oluşturup gönder
    sendNetworkStatus(code);
  }

  // BLE’i kapatmak için kontrol
  if (shouldStopBLE && ble_Connecting) {
    // Küçük bir bekleme koyarak tüm notify’lerin verinin çıkmasına izin verin
    delay(500);  
    Serial.println("Bluetooth devre dışı bırakılıyor...");
    BLEDevice::stopAdvertising();
    btStop();
    shouldStopBLE = false;
    ble_Connecting = false;
    delay(100);
    ESP.restart();
  }

  // 3) UART okuma
  if (!bleMode && SerialUART.available()) 
  {
    uint8_t buf[64] = {};
    size_t len = SerialUART.readBytes(buf, sizeof(buf));
    processIncoming(buf, len);

    Serial.print("Received: ");
    for (int i = 0; i < len; i++) {
      Serial.printf("%02X ", buf[i]); // Hex formatında yaz
    }
    Serial.println();
  }

  // 4) UART parse
  Frame f;
  while (parseNextFrame(&f)) 
  {
    //printf("Cmd: %d\n", f.command);
    switch (f.command) 
    {
      case Wifi_Reset1:// Wifi Sıfırla
      {

      }break;
      case Wifi_Reset2:// Wifi Sıfırla + Seçilebilir Eşleştirme Modu
      {
        Serial.printf(" WIFI RESET2 ✅\n");
        resetWiFi();

      }break;
      case Log_State:// Log Durumu bildieimi (Keypad + RFID + Fingerprint : ADMIN+USER)
      {
        if(f.data[0] == 0x08) // Hatalı Giriş ( f.data[f.length-1] = Keypad = 0x01 + RFID = 0x02 + Fingerprint = 0x00 )  
        {
          Serial.printf(" LOG STATE ✅\n");
          sendCommand(LOG, static_cast<LogType>(f.data[f.length-1]),"false");
        }
        else if(f.data[0] == 0x01 || f.data[0] == 0x02 || f.data[0] == 0x05) // Giriş Yapıldı (Keypad + RFID + Fingerprint : ADMIN+USER)
        {
          Serial.printf(" LOG STATE ✅\n");
          switch (f.data[0]) 
          {
            case 0x01://Fingerprint
            {
              if(f.data[f.length-1] == 1)//ADMIN
              {
                sendCommand(LOG, FINGERPRINT1,"true");
              }
              else //USER
              {
                sendCommand(LOG, FINGERPRINT1,"true");
              }
            }break;
            case 0x02://Keypad
            {
              if(f.data[f.length-1] == 0)//ADMIN
              {
                sendCommand(LOG, KEYPAD1,"true");
              }
              else //USER
              {
                sendCommand(LOG, KEYPAD1,"true");
              }
            }break;
            case 0x05://RFID
            {
              if(f.data[f.length-1] == 0)//ADMIN
              {
                sendCommand(LOG, RFID1,"true");
              }
              else //USER
              {
                sendCommand(LOG, RFID1,"true");
              }
            }break;
          }
        }
        else if(f.data[0] == 0x09)//Uzaktan Kilit Açma Geri Sayım
        {

        }
        else if(f.data[0] == 0x31)// Kodsuz Kilit Açma İsteği
        {
          if(!Remote_Open)
          {
            Remote_Open = true;
            remoteOpenTimestamp = millis();    // ← zamanı sakla
            Serial.printf(" OPEN DOOR ✅\n");
            sendCommand(OPEN);
          }
        }

      }break;
      case GMT_Time:// Saat güncelleme isteği
      {
        Serial.printf(" GMT TIME ✅\n");
      }break;
      case Temporary_Pass:// Geçici Parola isteği
      {

        String Pass_Value;
        for (size_t i = 0; i < f.length; i++) 
        {
            // Tek basamaklı rakamsa '0' + degeri -> karaktere çevirir
          Pass_Value += char('0' + f.data[i]);
        }

        sendCommand(PASS, KEYPAD1, Pass_Value);

          Serial.print("Password: ");
          Serial.println(Pass_Value);

        Serial.printf(" TEMPORARY PASSWORD ✅\n");
      }break;
      default:
        Serial.printf("Bilinmeyen CMD=0x%02X, Len=%u\n", f.command, f.length);
      break;
    }
    printf("Header: 0x%04X\n", f.header);
    printf("Ver: %u, Cmd: 0x%02X, Len: %u\n", f.version, f.command, f.length);
    printf("Data: 0x");
    for (int i = 0; i < f.length; i++) printf("%02X",f.data[i]);
    putchar('\n');
    printf("Checksum OK: 0x%02X\n", f.checksum);

    free(f.data); 
  }

}

void sendCommand(CommandType cmdType, LogType logType, String passValue) {
    
  if (commandInProgress) return;      // hâlihazırda bir komut bekliyorsa çık
  if (WiFi.status() != WL_CONNECTED)
  {
    if(cmdType == PASS)
    {
      sendStatusUART(0x05);// wifi not connecting = 0x05
    }
    return;
  }
  commandInProgress = true;           // artık cevap bekleniyor

  getServerTime();//saat güncelle

  // serverEpoch şimdi güncel epoch saniye. milis cinsinden ihtiyacımız varsa *1000:
  unsigned long long createdAt = serverMillis;//(unsigned long long)serverEpoch * 1000ULL;

  StaticJsonDocument<512> doc;
  JsonObject args = doc.createNestedObject("args");

  doc["action"] = "run";
  doc["name"] = "device";

  switch (cmdType) {
    case LOG:
      args["command"] = "log";
      args["extra"] =
        logType == RFID1 ? "RFID1" :
        logType == KEYPAD1 ? "Keypad1" : "FingerPrint1";
      if(passValue == "true")
      {
        args["status"] = "succeeded"; // succeeded
      }
      else
      {
        args["status"] = "failed"; // succeeded
      }
      break;

    case PASS:
      args["command"] = "open";
      //passValue = "1234567895"; // 4-10 haneli şifre
      args["extra"] = passValue;
      open_password_request = true;
      break;

    case OPEN:
      args["command"] = "open";
      open_request = true;
      break;
  }

  args["pairKey"] = storedPairKey;  
  args["deviceKey"] = storedDeviceKey;
  args["deviceId"] = device_id;
  args["createdAt"] = createdAt;

  String requestBody;
  serializeJson(doc, requestBody);

  http.begin(client, WEB_URL);
  http.addHeader("Content-Type", "application/json");

  int httpCode = http.POST(requestBody);
  String payload = http.getString();

  Serial.println("\nGönderilen Komut: " + String(args["command"].as<const char*>()));
  Serial.println("Request Body: " + requestBody);
  handleResponse(httpCode, payload);

  http.end();
  delay(250); // Her isteğin arasında kısa bir bekleme
}

void handleResponse(int httpCode, const String& payload) {
  Serial.print("HTTP Kodu: ");
  Serial.println(httpCode);
  Serial.print("Cevap: ");
  Serial.println(payload);

  StaticJsonDocument<256> resDoc;
  deserializeJson(resDoc, payload);

  if (httpCode == 200) 
  {
    Serial.println("✅ OK - İşlem başarılı");
    if (open_request) 
    {
      open_request = false ;
      Serial.println("🔓 Uygulamadan Kilit açıldı!");
      sendOpenRequestFrame();
    }
    else if (open_password_request) 
    {
      open_password_request = false ;
      sendStatusUART(0x01);
    }
  } 
  else if (httpCode == 403) 
  {
    Serial.println("⛔ Forbidden - Yetki hatası");
    if (open_password_request) 
    {
      open_password_request = false ;
      sendStatusUART(0x00);
    }
  } 
  else if (httpCode == 400) 
  {
    Serial.println("⚠️ Bad Request - Geçersiz parametre");
  } 
  else 
  {
      if (open_password_request) 
      {
        open_password_request = false ;
        sendStatusUART(0x00);
      }
    if (httpCode < 0) 
    {
      // -1, -2… vs. kodu ve açıklamasını birlikte yazdırın
      Serial.printf("❌ HTTPClient Hatası: kod=%d, mesaj=%s\n",
      httpCode, http.errorToString(httpCode).c_str());
    } 
    else 
    {
      String payload = http.getString();
      Serial.printf("✅ HTTP Kodu: %d, payload=%s\n", httpCode, payload.c_str());
    }
  }
  commandInProgress = false;
}

void resetWiFi() {
  factoryReset();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(20);
  Serial.println("🔁 Wi-Fi resetlendi!");
  startBLE();
  Serial.println("Karakteristik tanımlandı! Telefonunuzdan erişebilirsiniz.");
}

// Bir sonraki frame'i ayrıştırmaya çalışır
bool parseNextFrame(Frame* out) {
  if (stream_len < 7) return false;  // en küçük frame bile yok

  // 1) “55 AA” header’ını tara
  size_t i;
  for (i = 0; i + 1 < stream_len; ++i) {
    if (stream_buf[i] == 0x55 && stream_buf[i+1] == 0xAA) break;
  }
  if (i + 1 >= stream_len) {
    // header hiç yok, tamponu sıfırla
    stream_len = 0;
    return false;
  }
  // eğer header tamponun başında değilse, öncesini at
  if (i) {
    memmove(stream_buf, stream_buf + i, stream_len - i);
    stream_len -= i;
  }
  // en az header+vers+cmd+len(2)+checksum olmalı
  if (stream_len < 7) return false;

  // 2) alanları oku
  out->header  = (stream_buf[0] << 8) | stream_buf[1];
  out->version = stream_buf[2];
  out->command = stream_buf[3];
  out->length  = (stream_buf[4] << 8) | stream_buf[5];

  size_t full_len = 2 + 1 + 1 + 2 + out->length + 1;
  if (stream_len < full_len) return false; // henüz tam frame yok

  if(out->command == 0xDD)
  {
    // 3) Data’yı ayır, ASCII rakam karakterine çevir
    if (out->length) 
    {
      out->data = (uint8_t*)malloc(out->length + 1);
      for (size_t j = 0; j < out->length; ++j) 
      {
        uint8_t raw = stream_buf[6 + j] % 10; 
        // raw 0x00→0, 0x01→1, …, 0x09→9 varsayımıyla:
        out->data[j] = raw;
      }
  //    out->data[out->length] = '\0';  // null-terminator
    } 
    else 
    {
      out->data = NULL;
    }
    out->checksum = stream_buf[6 + out->length];
  }
  else
  {
    // 3) data’yı ayır ve oku
    if (out->length) {
      out->data = (uint8_t*)malloc(out->length);
      memcpy(out->data, stream_buf + 6, out->length);
    } else {
      out->data = NULL;
    }
    out->checksum = stream_buf[6 + out->length];
  }


  // 4) checksum kontrolü
  uint32_t sum = 0;
  for (size_t j = 0; j < 6 + out->length; ++j) sum += stream_buf[j];
  if ((uint8_t)(sum & 0xFF) != out->checksum) {
    // bozuksa header'ı atıp yeniden dene
    free(out->data);
    memmove(stream_buf, stream_buf + 2, --stream_len);
    return parseNextFrame(out);
  }

  // 5) tamponu kaydır
  memmove(stream_buf,
          stream_buf + full_len,
          stream_len - full_len);
  stream_len -= full_len;
  return true;
}
// Yeni gelen UART verisini işle
void processIncoming(const uint8_t* buf, size_t len) {
  // tampon taşma koruması
  if (stream_len + len > STREAM_SIZE) stream_len = 0;
  memcpy(stream_buf + stream_len, buf, len);
  stream_len += len;
}

uint8_t getWifiStateCode() {
  wl_status_t st = WiFi.status();
  switch (st) {
    //case WL_NO_SHIELD:      return 0x06; // örnek: QR kodlu eşleşme vb.
    case WL_IDLE_STATUS:    return 0x00; // EZ modu eşleştirme  //
    //case WL_SCAN_COMPLETED: return 0x00;
    case WL_CONNECT_FAILED: return 0x02;
    case WL_CONNECTION_LOST:return 0x02;
    case WL_DISCONNECTED:   return 0x02; // ağa yönlendirici bağlı değil
    case WL_CONNECTED:      return 0x04; // router’a ve buluta bağlı
  }
}
//   status = 0x01  → OK paketi (55 AA 00 DD 00 01 01 DE)  
//   status = 0x00 → ERROR paketi (55 AA 00 DD 00 01 00 D0)  
//   status = 0x00 → bot wifi connecting (55 AA 00 DD 00 01 05 E2)  
void sendStatusUART(uint8_t status) {
  uint8_t packet[8] = {
    0x55,       // Sabit başlık 1
    0xAA,       // Sabit başlık 2
    0x00,       // Sabit
    0xDD,       // Sabit
    0x00,       // Sabit
    0x01,       // Sabit
    status,     //Degisken
    0x00        //Degisken
  };

    if(status == 0x01)
    {
      packet[7] = 0xDE; 
    }
    else if(status == 0x00)
    {
      packet[7] = 0xDD; 
    }
    else if(status == 0x05)
    {
      packet[7] = 0xE2; 
    }

    // --- packet içeriğini hex olarak yazdır ---
  for(size_t i = 0; i < sizeof(packet); ++i) 
  {
    uint8_t b = packet[i];
    // tek haneli hex için başına '0' koy
    if (b < 0x10) Serial.print('0');
    Serial.print(b, HEX);
    Serial.print(' ');
  }
  Serial.println();  // satır sonu

  // Gönder
  SerialUART.write(packet, sizeof(packet));

}
/**
 * Ağ durumu frame'ini oluşturup UART1'e yazar.
 */
void sendNetworkStatus(uint8_t statusCode) 
{
  // frame boyutu = header(2)+ver(1)+cmd(1)+len(2)+data(1)+chk(1) = 8
  uint8_t buf[8];

  // Header big-endian
  buf[0] = 0x55;
  buf[1] = 0xAA;

  buf[2] = Version;
  buf[3] = Wifi_Status;

  // Data length = 0x0001
  buf[4] = (Wifi_Status_Data_Len >> 8) & 0xFF;
  buf[5] =  Wifi_Status_Data_Len       & 0xFF;

  // Tek bayt data: durum kodu
  buf[6] = statusCode;

  // Checksum = sum(buf[0]..buf[6]) mod 256
  uint16_t sum = 0;
  for (int i = 0; i <= 6; ++i) sum += buf[i];
  buf[7] = sum & 0xFF;

  // Gönder
  SerialUART.write(buf, sizeof(buf));
}

void sendOpenRequestFrame() 
{
  // Kapı açma frame'i:
  const uint8_t frame[] = {
    0x55, 0xAA,       // Header
    0x00,             // Version
    0x09,             // Cmd = 0x09 (örnek)
    0x00, 0x11,       // Length = 0x0011 (17 byte data)
    0x32, 0x00, 0x00, 0x0D, 0x01,
    0x00, 0x01,
    0x36, 0x33, 0x30, 0x33, 0x33, 0x36, 0x37, 0x33,
    0x00,             // sonlandırıcı ya da padding
    0x01,             // ekstra data
    0xFA              // Checksum
  };

    // Gönder
  SerialUART.write(frame, sizeof(frame));
}

void sendChunkedData(String data) {
  // Bağlantı kontrolü
  if (!deviceConnected) {
    Serial.println("⚠️ Device not connected, cannot send data");
    return;
  }

  // iOS için güvenli MTU boyutu: 19 byte (20 byte limit, 1 byte overhead için)
  const int MTU_PAYLOAD = 19;
  // iOS için daha uzun gecikme gerekli
  const unsigned DELAY_MS = 50;

  int len = data.length();
  Serial.printf("Data length: %d\n", len);
  Serial.printf("Full response: %s\n", data.c_str());

  // İlk chunk'ın başında { olduğundan emin ol
  if (data.length() > 0 && data.charAt(0) != '{') {
    Serial.println("⚠️ WARNING: Response doesn't start with {");
    return;
  }

  // İlk gönderimden önce kısa bekleme (bağlantı stabilizasyonu)
  delay(100);

  for (int pos = 0; pos < len; pos += MTU_PAYLOAD) {
    int chunkSize = min(MTU_PAYLOAD, len - pos);
    bool isLast   = (pos + chunkSize >= len);

    // Orijinal veri parçasını al
    String chunk = data.substring(pos, pos + chunkSize);

    if (isLast) {
      // Son parçaya terminatör ekle
      chunk += char(0x00);
    }

    // Notify ile gönderilecek gerçek uzunluk:
    int sendLen = chunk.length();

    // Ayar ve iletim
    pCharacteristic->setValue((uint8_t*)chunk.c_str(), sendLen);
    pCharacteristic->notify();

    // iOS için daha uzun bekleme ve task yield
    taskYIELD();
    delay(DELAY_MS);

    Serial.print("Sent chunk ");
    Serial.print((pos / MTU_PAYLOAD) + 1);
    Serial.print(" (pos=");
    Serial.print(pos);
    Serial.print(", len=");
    Serial.print(sendLen);
    Serial.print("): ");
    // İlk 50 karakteri göster
    if (chunk.length() > 50) {
      Serial.print(chunk.substring(0, 50));
      Serial.println("...");
    } else {
      Serial.println(chunk);
    }

    if (isLast) {
      Serial.println("→ All data sent with terminator.");
      // Son chunk'tan sonra ekstra bekleme
      delay(50);
      break;
    }
  }
}

/*void sendChunkedData(String data) {

  int len = data.length();
  Serial.print("Data lenght: ");
  Serial.println(len);

  for (int pos = 0; pos < len; pos += 20) {
    int chunkSize = min(20, len - pos);
    bool isLast   = (pos + chunkSize >= len);
    String chunk  = data.substring(pos, pos + chunkSize);

    if (isLast) {
      if (chunkSize < 20) {

        chunk += char(0x00);
        pCharacteristic->setValue((uint8_t*)chunk.c_str(), chunk.length());
        pCharacteristic->notify();
        taskYIELD();
//        Serial.println(chunk.c_str());
      } else {
        pCharacteristic->setValue((uint8_t*)chunk.c_str(), chunk.length());
        pCharacteristic->notify();
        taskYIELD();
//        Serial.println(chunk.c_str());
        delay(20);
        uint8_t terminator = 0x00;
        pCharacteristic->setValue(&terminator, 1);
        pCharacteristic->notify();
        taskYIELD();
//        Serial.println(chunk.c_str());
        delay(20);

      }
    } else {
      pCharacteristic->setValue((uint8_t*)chunk.c_str(), chunk.length());
      pCharacteristic->notify();
      taskYIELD();
//      Serial.println(chunk.c_str());
    }
    delay(20);
  }
}*/

void sendData(String data) {
  Serial.print("Gönderilen Veri: ");
  Serial.println(data);

  Write_Data_Flag = true;
//  sendChunkedData(data);
}

bool connectToWiFi(const char* ssid, const char* password) {
  Serial.print("WiFi'ye baglaniliyor: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  unsigned long startTime = millis();

  while (WiFi.status() != WL_CONNECTED && (millis() - startTime) < 15000) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi bağlantısı başarılı!");
    Serial.print("IP adresi: ");
    Serial.println(WiFi.localIP());

    SerialUART.begin(BAUD_RATE, SERIAL_8N1, RX_PIN, TX_PIN);
    bleMode = false;

    return true;
  } else {
    Serial.println("WiFi bağlantısı kurulamadı!");
    return false;
  }
}

bool loadWiFiCredentials(String &ssid, String &pass) {
  preferences.begin("wifi", true);
  ssid = preferences.getString("ssid", "");
  pass = preferences.getString("pass", "");
  preferences.end();
  return ssid.length() > 0 && pass.length() > 0;
}

void saveWiFiCredentials(const char* ssid, const char* pass) {
  preferences.begin("wifi", false);
  preferences.putString("ssid", ssid);
  preferences.putString("pass", pass);
  preferences.end();
}

void clearWiFiCredentials() {
  preferences.begin("wifi", false);
  preferences.clear();
  preferences.end();
}

void saveDeviceKeys(const char* deviceKey, const char* pairKey) {
  preferences.begin("keys", false);
  preferences.putString("deviceKey", deviceKey);
  preferences.putString("pairKey", pairKey);
  preferences.end();
}

bool loadDeviceKeys(String &deviceKey, String &pairKey) {
  preferences.begin("keys", true);
  deviceKey = preferences.getString("deviceKey", "");
  pairKey   = preferences.getString("pairKey", "");
  preferences.end();
  return deviceKey.length() > 0 && pairKey.length() > 0;
}

void clearDeviceKeys() {
  preferences.begin("keys", false);
  preferences.clear();
  preferences.end();
}

void factoryReset() {
  clearWiFiCredentials();
  clearDeviceKeys();
  ESP.restart();
}

// BLE veri alma callback
class MyCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pChar) override {

    String rx = pChar->getValue();
    int len = rx.length();
    Serial.print("Data Chunk Length: ");
    Serial.println(len);

    for (int i = 0; i < len; ++i) {
      char c = rx.charAt(i);
      if (c == '\0') {
        // Tam mesaj alındı
        Serial.println("📩 Tam mesaj alındı:");
        Serial.println(incomingBuffer);

        // JSON parse
        StaticJsonDocument<4096> doc;
        DeserializationError error = deserializeJson(doc, incomingBuffer);
        if (error) {
          Serial.print("JSON Hatası: ");
          Serial.println(error.c_str());
          incomingBuffer ="";
          return;
        }

        const char* command = doc["command"];
        if (!command) {
          Serial.println("Komut bulunamadı.");
          incomingBuffer ="";
          return;
        }

        if (strcmp(command, "handshake") == 0) {
          response = "{\"command\":\"handshake\",\"deviceId\":\"" + String(device_id) + "\",\"createdAt\":" + String(millis()) + "}";
//            response = "AB";
        }
        else if (strcmp(command, "pair") == 0) {
          const char* deviceKey = doc["deviceKey"];
          const char* pairKey   = doc["pairKey"];
          if (deviceKey && pairKey) {
            saveDeviceKeys(deviceKey, pairKey);
            storedDeviceKey = String(deviceKey);
            storedPairKey   = String(pairKey);
            Serial.println(" deviceKey ve pairKey Kayıt edildi.");
            response = "{\"command\":\"pair\",\"result\":\"succeeded\"}";
          } else {
            response = "{\"command\":\"pair\",\"result\":\"failed\"}";
          }
        }
        else if (strcmp(command, "getSsids") == 0) {
          Serial.println("WiFi taraması başlatılıyor...");

          // BLE’den bağımsız olarak Wi-Fi donanımını hazırla
          WiFi.mode(WIFI_STA);            
          WiFi.disconnect(true);          // Önceki bağlantıları temizle
          delay(100);                     // Donanımın sıfırlanması için kısa bekleme

          int n = WiFi.scanNetworks();
          Serial.print(n);
          Serial.println(" ağ bulundu.");

          StaticJsonDocument<6144> jsonDoc;
          jsonDoc["command"] ="getSsids";
          JsonObject resultObj = jsonDoc.createNestedObject("result");
          for (int j = 0; j < n; ++j) {
            JsonObject ap = resultObj.createNestedObject(String(j));
            ap["macAddress"] = WiFi.BSSIDstr(j);
            ap["rssi"]       = WiFi.RSSI(j);
            int channel       = WiFi.channel(j);
            ap["frequency"]  = (channel < 15) ? "2.4" : "5";
            ap["name"]       = WiFi.SSID(j);
          }
          serializeJson(jsonDoc, response);
          WiFi.scanDelete();
        }
        else if (strcmp(command, "connectWifi") == 0) {
          const char* ssid       = doc["ssid"];
          const char* pass       = doc["pass"];
          const char* macAddress = doc["macAddress"];
          if (ssid && pass && macAddress) {
            if (connectToWiFi(ssid, pass)) {
              wifiConnected = true;
              saveWiFiCredentials(ssid, pass);

          // SSL sertifikası
          client.setCACert(rootCACertificate);
          client.setInsecure(); // Sertifika doğrulamasını devre dışı bırak (Geliştirme için)

          // HTTP zaman aşımı
          http.setTimeout(15000);
              response = "{\"command\":\"connectWifi\",\"result\":\"succeeded\"}";
              shouldStopBLE = true;

            } else {
              response = "{\"command\":\"connectWifi\",\"result\":\"failed\"}";
            }
          } else {
            response = "{\"command\":\"connectWifi\",\"result\":\"missing_credentials\"}";
          }
        }
        else {
          response = "{\"command\":\"error\",\"result\":\"unknown command\"}";
        }
        sendData(response);
        incomingBuffer = "";
      } else {
        incomingBuffer += c;
      }
    }

    lastReceiveTime = millis();
    
  }

};

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) override {
    deviceConnected = true;
    Serial.println("🔌 Cihaz bağlandı.");
    // MTU görüşmesi için istek gönder (iOS için önemli)
    // Not: Gerçek MTU client tarafından belirlenir, bu sadece istek
    delay(100); // Bağlantı stabilizasyonu için bekle
  }

  void onDisconnect(BLEServer* pServer) override {
    deviceConnected = false;
    Serial.println("🔌 Cihaz bağlantısı kesildi.");
    BLEDevice::startAdvertising();  // Bağlantı kesilince yeniden reklam ver
  }
};

void startBLE() {

  BLEDevice::init("Onloi Lock");
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());  // Server callback set
  BLEService *pService = pServer->createService(SERVICE_UUID);

  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_WRITE |
    BLECharacteristic::PROPERTY_NOTIFY
  );

  // iOS için BLE2902 descriptor ekle (notifications için gerekli)
  pCharacteristic->addDescriptor(new BLE2902());

  pCharacteristic->setCallbacks(new MyCallbacks());
  pCharacteristic->setValue("Onloi Lock Ready");

  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();

  bleMode = true;
  SerialUART.end();

}

bool getServerTime() {
  StaticJsonDocument<256> req;
  req["action"] = "run";
  req["name"]   = "device";
  JsonObject args = req.createNestedObject("args");
  args["command"]   = "getTime";
  args["pairKey"]   = storedPairKey;
  args["deviceKey"] = storedDeviceKey;
  args["deviceId"]  = device_id;

  String body;
  serializeJson(req, body);

  http.begin(client, WEB_URL);
  http.addHeader("Content-Type", "application/json");
  int code = http.POST(body);
  if (code != 200) {
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();
  payload.trim();
  Serial.print("Cevap (raw): "); Serial.println(payload);

  bool allDigits = payload.length() > 0;
  for (size_t i = 0; i < payload.length(); ++i) {
    if (!isDigit(payload.charAt(i))) { allDigits = false; break; }
  }

  if (allDigits) {
    unsigned long long ts = strtoull(payload.c_str(), nullptr, 10);
    serverMillis = ts;
    serverEpoch  = (time_t)(ts / 1000ULL);
    Serial.printf("⏱️ ms: %llu -> s: %u\n", ts, (unsigned)serverEpoch);
  } else {
    StaticJsonDocument<256> res;
    if (deserializeJson(res, payload)) return false;
    long long raw = res["result"]["epoch"].as<long long>();
    if (raw > 10000000000LL) {
      serverMillis = raw;
      serverEpoch  = (time_t)(raw / 1000LL);
    } else {
      serverEpoch  = (time_t)raw;
      serverMillis = ((unsigned long long)serverEpoch) * 1000ULL;
    }
    Serial.printf("⏱️ JSON -> ms: %llu, s: %u\n", serverMillis, (unsigned)serverEpoch);
  }

  struct timeval tv = { .tv_sec = serverEpoch, .tv_usec = 0 };
  settimeofday(&tv, nullptr);
  Serial.printf("✅ Saat güncellendi: %s", ctime(&serverEpoch));
  return true;
}
