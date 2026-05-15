#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "led_strip.h"
#include <stdio.h>

// ESP32-C6-DevKitC-1 V1.2 üzerinde dahili RGB LED (WS2812) GPIO 8'e bağlıdır.
#define BLINK_GPIO 8

static const char *TAG = "RTOS_DERS_1";

// RMT tabanlı Addressable LED Strip objesi
static led_strip_handle_t led_strip;

// --- MUTEX TANIMLAMASI ---
// Ortak donanıma (RGB LED) aynı anda birden fazla görevin yazmasını
// engelleyecek olan koruyucu kilit (Mutex).
static SemaphoreHandle_t led_mutex = NULL;

static void configure_led(void) {
  ESP_LOGI(TAG, "Dahili adreslenebilir RGB LED (WS2812) yapilandiriliyor...");

  // LED donanım ayarları
  // ESP-IDF v4 ve v5 geçişlerinde led_strip_config_t yapısı değiştiği için
  // eski/yeni yapıları destekleyecek sade bir konfigürasyon yapıyoruz.
  led_strip_config_t strip_config = {
      .strip_gpio_num = BLINK_GPIO, // LED'in bağlı olduğu GPIO
      .max_leds = 1,                // Kartta 1 tane LED var
  };

  // LED sinyallerini sürecek olan RMT çevrebirimi ayarları
  led_strip_rmt_config_t rmt_config = {
      .resolution_hz = 0, // 10MHz sinyal çözünürlüğü
  };

  ESP_ERROR_CHECK(
      led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));

  // Açılışta LED'i temizle (söndür)
  led_strip_clear(led_strip);
}

// --- LED-1 Görev Fonksiyonu ---
void led1_task(void *pvParameter) {
  bool led_durumu = false;

  while (1) {
    // 1. ADIM: Mutex'i almayı (Kilitlemeyi) dene.
    // portMAX_DELAY: Eğer Mutex (LED) şu an başka bir görev tarafından
    // kullanılıyorsa, sonsuza kadar bekle!
    if (xSemaphoreTake(led_mutex, portMAX_DELAY) == pdTRUE) {

      // Mutex'i aldık! Artık LED donanımı sadece bize ait.
      if (led_durumu) {
        led_strip_set_pixel(led_strip, 0, 16, 16, 0); // Sarı renk
        led_strip_refresh(led_strip);
        ESP_LOGI(TAG, "SARI LED YANDI!");
      } else {
        led_strip_clear(led_strip);
        ESP_LOGI(TAG, "SARI LED SONDU!");
      }

      led_durumu = !led_durumu;

      // 2. ADIM: İşimiz bitti, Mutex'i (Kilidi) geri ver ki diğer görevler
      // LED'i kullanabilsin.
      xSemaphoreGive(led_mutex);
    }

    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

// --- LED-2 Görev Fonksiyonu ---
void led2_task(void *pv2Parameter) {
  bool led_durumu = false;

  while (1) {
    // 1. ADIM: Mutex'i almayı (Kilitlemeyi) dene.
    if (xSemaphoreTake(led_mutex, portMAX_DELAY) == pdTRUE) {

      if (led_durumu) {
        led_strip_set_pixel(led_strip, 0, 0, 16, 16); // Turkuaz renk
        led_strip_refresh(led_strip);
        ESP_LOGI(TAG, "TURKUAZ LED YANDI!");
      } else {
        led_strip_clear(led_strip);
        ESP_LOGI(TAG, "TURKUAZ LED SONDU!");
      }

      led_durumu = !led_durumu;
      xSemaphoreGive(led_mutex);
    }

    vTaskDelay(5000 / portTICK_PERIOD_MS);
  }
}

// --- Ana Fonksiyon ---
void app_main(void) {
  ESP_LOGI(TAG,
           "Gomulu RTOS Seruveni Basliyor! Hedef: ESP32-C6 (Mutex Ornegi)");

  // 1. ADIM: Donanımı sadece 1 KERE, görevler (Task) oluşturulmadan ÖNCE
  // yapılandırıyoruz.
  configure_led();

  // 2. ADIM: Mutex'i yaratıyoruz.
  led_mutex = xSemaphoreCreateMutex();

  if (led_mutex != NULL) {
    // Mutex başarıyla yaratıldıysa görevleri başlatıyoruz.
    xTaskCreate(led1_task, "SariGorevi", 4096, NULL, 5, NULL);

    // Turkuaz görevine daha DÜŞÜK bir öncelik (4) verdik.
    xTaskCreate(led2_task, "TurkuazGorevi", 4096, NULL, 4, NULL);
  } else {
    ESP_LOGE(TAG, "Mutex yaratilamadi, sistem durduruldu!");
  }
}
