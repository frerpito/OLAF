#include "esp_err.h"
#include "esp_log.h"
#include "app_controller.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    ESP_LOGI(TAG, "================================");
    ESP_LOGI(TAG, "OLAF - Iniciando sistema");
    ESP_LOGI(TAG, "================================");

    esp_err_t err = app_controller_init();

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

    ESP_LOGI(TAG, "Sistema inicializado com sucesso");
}