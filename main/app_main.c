#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "esp_log.h"

#include "app_controller.h"


static const char *TAG = "MAIN";


void app_main(void)
{
    ESP_LOGI(TAG, "================================");
    ESP_LOGI(TAG, "OLAF - Iniciando sistema");
    ESP_LOGI(TAG, "================================");

    /*
     * Inicializa os módulos utilizados pelo sistema:
     *
     * - sensor da porta;
     * - sensor de temperatura;
     * - LED;
     * - buzzer;
     * - timeout adaptativo.
     */
    esp_err_t err = app_controller_init();

    if (err != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Falha ao inicializar o sistema: %s",
            esp_err_to_name(err)
        );

        return;
    }

    ESP_LOGI(TAG, "Sistema inicializado com sucesso");

    /*
     * Cria a tarefa principal responsável pelo monitoramento
     * contínuo da câmara frigorífica.
     *
     * Essa tarefa executará:
     *
     * 1. leitura do sensor da porta;
     * 2. leitura periódica da temperatura;
     * 3. cálculo do tempo de porta aberta;
     * 4. verificação do timeout;
     * 5. controle do LED;
     * 6. controle do buzzer;
     * 7. acompanhamento da recuperação térmica;
     * 8. atualização do timeout adaptativo.
     */
    BaseType_t task_created = xTaskCreate(
        app_controller_task,     // função executada
        "olaf_controller",       // nome da tarefa
        6144,                    // tamanho da stack
        NULL,                    // parâmetros
        5,                       // prioridade
        NULL                     // handle da tarefa
    );

    if (task_created != pdPASS) {
        ESP_LOGE(
            TAG,
            "Nao foi possivel criar a tarefa principal"
        );

        return;
    }

    ESP_LOGI(
        TAG,
        "Tarefa principal criada com sucesso"
    );
}