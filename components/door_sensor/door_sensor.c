#include "door_sensor.h"
#include "app_config.h"

#include "driver/gpio.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"


static const char *TAG = "DOOR_SENSOR";


/* =========================================================
 * ESTADO INTERNO DO SENSOR
 * ========================================================= */

/*
 * Estado lógico atualmente aceito pelo sistema.
 */
static door_state_t s_state = DOOR_STATE_CLOSED;


/*
 * Último nível elétrico lido no GPIO.
 *
 * Exemplo:
 *
 * 0 ou 1
 *
 * Não confundir com DOOR_STATE_OPEN / CLOSED.
 */
static int s_last_raw_level = OLAF_DOOR_CLOSED_LEVEL;


/*
 * Momento em que o nível elétrico mudou pela última vez.
 *
 * Utilizado para implementar o debounce/filtro temporal.
 */
static int64_t s_raw_changed_at_us = 0;


/* =========================================================
 * CONVERSÃO DO SINAL ELÉTRICO PARA ESTADO DA PORTA
 * ========================================================= */

/**
 * @brief Converte o nível lógico do GPIO para o estado
 *        físico da porta.
 */
static door_state_t raw_level_to_door_state(int raw_level)
{
    /*
     * Se o nível corresponde ao nível configurado
     * como "porta fechada", então consideramos que
     * o E18-D80NK está detectando a chapa.
     */

    if (raw_level == OLAF_DOOR_CLOSED_LEVEL)
    {
        return DOOR_STATE_CLOSED;
    }


    /*
     * Caso contrário, consideramos que a chapa
     * saiu da região de detecção.
     */

    return DOOR_STATE_OPEN;
}


/* =========================================================
 * INICIALIZAÇÃO
 * ========================================================= */

esp_err_t door_sensor_init(void)
{
    /*
     * Configuração do GPIO utilizado pelo E18-D80NK.
     */

    gpio_config_t io_config =
    {
        .pin_bit_mask = (1ULL << OLAF_DOOR_GPIO),

        .mode = GPIO_MODE_INPUT,

        /*
         * Pull-up e pull-down ficam inicialmente
         * desabilitados.
         *
         * A configuração elétrica definitiva depende
         * de como a saída do E18-D80NK será adaptada
         * para os 3,3 V da ESP32-S3.
         */
        .pull_up_en = GPIO_PULLUP_DISABLE,

        .pull_down_en = GPIO_PULLDOWN_DISABLE,

        /*
         * Neste projeto não precisamos de interrupção
         * para o sensor da porta.
         *
         * A tarefa principal verifica o GPIO
         * periodicamente.
         */
        .intr_type = GPIO_INTR_DISABLE
    };


    /*
     * Configura o GPIO.
     */

    esp_err_t err =
        gpio_config(&io_config);


    if (err != ESP_OK)
    {
        ESP_LOGE(
            TAG,
            "Erro ao configurar GPIO do sensor: %s",
            esp_err_to_name(err)
        );

        return err;
    }


    /* =====================================================
     * LEITURA DO ESTADO INICIAL
     * ===================================================== */

    s_last_raw_level =
        gpio_get_level(
            OLAF_DOOR_GPIO
        );


    /*
     * Converte a leitura elétrica inicial para
     * ABERTA ou FECHADA.
     */

    s_state =
        raw_level_to_door_state(
            s_last_raw_level
        );


    /*
     * Registra o instante inicial.
     */

    s_raw_changed_at_us =
        esp_timer_get_time();


    ESP_LOGI(
        TAG,
        "Sensor da porta inicializado"
    );


    ESP_LOGI(
        TAG,
        "GPIO: %d",
        OLAF_DOOR_GPIO
    );


    ESP_LOGI(
        TAG,
        "Nivel inicial: %d",
        s_last_raw_level
    );


    ESP_LOGI(
        TAG,
        "Estado inicial: %s",
        s_state == DOOR_STATE_OPEN
            ? "ABERTA"
            : "FECHADA"
    );


    return ESP_OK;
}


/* =========================================================
 * ATUALIZAÇÃO DO SENSOR
 * ========================================================= */

door_state_t door_sensor_update(void)
{
    /*
     * Lê o nível elétrico atual do E18-D80NK.
     */

    const int raw_level =
        gpio_get_level(
            OLAF_DOOR_GPIO
        );


    /*
     * Obtém o tempo atual.
     */

    const int64_t now_us =
        esp_timer_get_time();


    /* =====================================================
     * DETECÇÃO DE ALTERAÇÃO ELÉTRICA
     * ===================================================== */

    if (raw_level != s_last_raw_level)
    {
        /*
         * O sinal mudou.
         *
         * Ainda NÃO alteramos imediatamente
         * o estado da porta.
         *
         * Primeiro esperamos para verificar
         * se esse novo sinal permanece estável.
         */

        s_last_raw_level =
            raw_level;


        /*
         * Guarda o momento em que a mudança
         * começou.
         */

        s_raw_changed_at_us =
            now_us;
    }


    /* =====================================================
     * TEMPO DE ESTABILIDADE
     * ===================================================== */

    const int64_t stable_time_us =
        now_us -
        s_raw_changed_at_us;


    /*
     * Converte o debounce configurado em
     * milissegundos para microssegundos.
     */

    const int64_t debounce_us =
        (int64_t)
        OLAF_DOOR_DEBOUNCE_MS
        * 1000LL;


    /* =====================================================
     * VALIDAÇÃO DA MUDANÇA
     * ===================================================== */

    if (stable_time_us >= debounce_us)
    {
        /*
         * O sinal permaneceu estável pelo período
         * configurado.
         *
         * Agora podemos considerá-lo válido.
         */

        const door_state_t new_state =
            raw_level_to_door_state(
                raw_level
            );


        /*
         * Só atualizamos e mostramos no log
         * caso o estado físico tenha realmente mudado.
         */

        if (new_state != s_state)
        {
            s_state =
                new_state;


            ESP_LOGI(
                TAG,
                "Estado da porta alterado: %s",
                s_state == DOOR_STATE_OPEN
                    ? "ABERTA"
                    : "FECHADA"
            );
        }
    }


    /*
     * Retorna sempre o último estado
     * considerado confiável.
     */

    return s_state;
}


/* =========================================================
 * CONSULTA DO ESTADO
 * ========================================================= */

door_state_t door_sensor_get_state(void)
{
    return s_state;
}