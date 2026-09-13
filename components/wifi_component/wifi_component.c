#include "wifi_component.h"

#include <string.h>

#include "esp_event.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define WIFI_MAX_RETRY     10

static const char *TAG = "wifi_component";

static EventGroupHandle_t wifi_event_group;
static int retry_count;
static bool connected;

static esp_err_t wifi_init_nvs(void)
{
    esp_err_t err = nvs_flash_init();

    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }

    return err;
}

static void wifi_event_handler(
    void *arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data)
{
    (void)arg;

    if (event_base == WIFI_EVENT &&
        event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        return;
    }

    if (event_base == WIFI_EVENT &&
        event_id == WIFI_EVENT_STA_DISCONNECTED) {
        connected = false;

        if (retry_count < WIFI_MAX_RETRY) {
            retry_count++;
            esp_wifi_connect();
            ESP_LOGW(TAG, "Tentando reconectar ao Wi-Fi (%d/%d)", retry_count, WIFI_MAX_RETRY);
        } else {
            xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
            ESP_LOGE(TAG, "Falha ao conectar ao Wi-Fi");
        }

        return;
    }

    if (event_base == IP_EVENT &&
        event_id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *event = event_data;

        retry_count = 0;
        connected = true;

        ESP_LOGI(TAG, "Wi-Fi conectado");
        ESP_LOGI(TAG, "IP: " IPSTR, IP2STR(&event->ip_info.ip));

        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

esp_err_t wifi_connect(void)
{
    if (strlen(CONFIG_WIFI_SSID) == 0) {
        ESP_LOGE(TAG, "CONFIG_WIFI_SSID vazio. Configure em idf.py menuconfig.");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_RETURN_ON_ERROR(wifi_init_nvs(), TAG, "Falha ao inicializar NVS");
    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "Falha ao inicializar esp_netif");
    ESP_RETURN_ON_ERROR(esp_event_loop_create_default(), TAG, "Falha ao criar event loop");

    wifi_event_group = xEventGroupCreate();

    if (wifi_event_group == NULL) {
        return ESP_ERR_NO_MEM;
    }

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();

    ESP_RETURN_ON_ERROR(esp_wifi_init(&init_config), TAG, "Falha ao inicializar Wi-Fi");

    ESP_RETURN_ON_ERROR(
        esp_event_handler_instance_register(
            WIFI_EVENT,
            ESP_EVENT_ANY_ID,
            wifi_event_handler,
            NULL,
            NULL),
        TAG,
        "Falha ao registrar eventos Wi-Fi");

    ESP_RETURN_ON_ERROR(
        esp_event_handler_instance_register(
            IP_EVENT,
            IP_EVENT_STA_GOT_IP,
            wifi_event_handler,
            NULL,
            NULL),
        TAG,
        "Falha ao registrar eventos IP");

    wifi_config_t wifi_config = {0};

    strlcpy(
        (char *)wifi_config.sta.ssid,
        CONFIG_WIFI_SSID,
        sizeof(wifi_config.sta.ssid));

    strlcpy(
        (char *)wifi_config.sta.password,
        CONFIG_WIFI_PASSWORD,
        sizeof(wifi_config.sta.password));

    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;

    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "Falha ao definir modo STA");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &wifi_config), TAG, "Falha ao configurar Wi-Fi");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "Falha ao iniciar Wi-Fi");

    ESP_LOGI(TAG, "Conectando ao Wi-Fi: %s", CONFIG_WIFI_SSID);

    EventBits_t bits = xEventGroupWaitBits(
        wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE,
        pdFALSE,
        portMAX_DELAY);

    if ((bits & WIFI_CONNECTED_BIT) != 0) {
        return ESP_OK;
    }

    return ESP_FAIL;
}

bool wifi_is_connected(void)
{
    return connected;
}
