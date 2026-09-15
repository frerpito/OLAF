#include "esp_err.h"
#include "esp_log.h"
#include "app_controller.h"
#include "mqtt_component.h"
#include "sdkconfig.h"
#include "wifi_component.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    ESP_LOGI(TAG, "================================");
    ESP_LOGI(TAG, "OLAF - Iniciando sistema");
    ESP_LOGI(TAG, "================================");

    esp_err_t err = wifi_connect();

    if (err != ESP_OK) {
        ESP_LOGW(
            TAG,
            "Falha ao conectar no Wi-Fi: %s",
            esp_err_to_name(err)
        );

        ESP_LOGW(
            TAG,
            "Sistema local sera iniciado sem MQTT"
        );
    }

    else {
        ESP_LOGI(TAG, "Wi-Fi conectado com sucesso");

        mqtt_config_t mqtt_config = {
            .broker_uri = CONFIG_MQTT_BROKER_URI,
            .username = NULL,
            .password = NULL,
            .auto_reconnect = true,
            .will_topic = "olaf/status",
            .will_message = "offline",
            .will_qos = 1,
            .will_retain = true,
        };

        err = mqtt_init(&mqtt_config);

        if (err != ESP_OK) {
            ESP_LOGW(
                TAG,
                "Falha ao inicializar MQTT: %s",
                esp_err_to_name(err)
            );
        }

        else {
            err = mqtt_start();

            if (err != ESP_OK) {
                ESP_LOGW(
                    TAG,
                    "Falha ao iniciar MQTT: %s",
                    esp_err_to_name(err)
                );
            }

            else {
                ESP_LOGI(TAG, "MQTT iniciado");
            }
        }
    }

    err = app_controller_init();

    if (err != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Falha ao inicializar o sistema: %s",
            esp_err_to_name(err)
        );
        return;
    }

    err = app_controller_start();

    if (err != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Falha ao iniciar o controlador: %s",
            esp_err_to_name(err)
        );
        return;
    }

    ESP_LOGI(TAG, "Sistema local inicializado com sucesso");
}
