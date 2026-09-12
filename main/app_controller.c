#include "app_controller.h"

#include <stdbool.h>
#include <stdint.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_timer.h"

#include "app_config.h"
#include "door_sensor.h"
#include "temperature_ntc.h"
#include "alarm.h"
#include "adaptive_timeout.h"


static const char *TAG = "OLAF";


/* =========================================================
 * ESTADO GERAL DA APLICAÇÃO
 * ========================================================= */

typedef struct
{
    /*
     * Estado atual e anterior da porta.
     *
     * Manter o estado anterior permite detectar eventos:
     *
     * FECHADA -> ABERTA
     * ABERTA  -> FECHADA
     */
    door_state_t door;
    door_state_t previous_door;


    /*
     * Última leitura válida do NTC.
     */
    temperature_reading_t temp;


    /*
     * Indica se existe alarme térmico ativo.
     */
    bool temp_alarm;


    /*
     * Momento da última leitura de temperatura.
     */
    int64_t last_temp_read_us;


    /*
     * Controle do temporizador da porta.
     */
    bool door_timer_active;

    int64_t door_opened_us;


    /*
     * Timeout calculado para a abertura atual.
     *
     * Esse valor é congelado no momento em que
     * a porta abre.
     */
    float opening_timeout_s;


    /*
     * Temperatura média da câmara antes da abertura.
     *
     * Será utilizada posteriormente para verificar
     * quanto tempo a câmara levou para recuperar.
     */
    float pre_open_baseline_c;


    /*
     * Estrutura responsável pelo aprendizado
     * do comportamento térmico.
     */
    adaptive_timeout_t adaptive;

} olaf_state_t;


/*
 * Estado global privado deste módulo.
 */

static olaf_state_t s;


/* =========================================================
 * ALARME DE TEMPERATURA
 * ========================================================= */

static bool update_temperature_alarm(
    bool previous_alarm,
    float temperature_c)
{
    /*
     * Uma leitura inválida é considerada condição anormal.
     *
     * É uma decisão fail-safe:
     *
     * se não conseguimos confiar no sensor,
     * não assumimos que tudo está normal.
     */
    if (!isfinite(temperature_c))
    {
        return true;
    }


    /*
     * Ativação do alarme.
     */

    if (!previous_alarm &&
        temperature_c >= OLAF_TEMP_ALARM_HIGH_C)
    {
        ESP_LOGW(
            TAG,
            "Alarme de temperatura ativado: %.2f C",
            temperature_c
        );

        return true;
    }


    /*
     * Desativação utilizando histerese.
     */

    if (previous_alarm &&
        temperature_c <= OLAF_TEMP_ALARM_CLEAR_C)
    {
        ESP_LOGI(
            TAG,
            "Temperatura retornou a faixa segura: %.2f C",
            temperature_c
        );

        return false;
    }


    return previous_alarm;
}


/* =========================================================
 * EVENTO: PORTA ABRIU
 * ========================================================= */

static void on_door_opened(int64_t now_us)
{
    /*
     * Inicia a contagem do tempo.
     */

    s.door_timer_active = true;

    s.door_opened_us = now_us;


    /*
     * Guarda a temperatura média de referência
     * existente antes da abertura.
     *
     * Essa temperatura será utilizada depois para
     * verificar a recuperação térmica.
     */

    if (s.adaptive.baseline_valid)
    {
        s.pre_open_baseline_c =
            s.adaptive.baseline_temp_c;
    }
    else
    {
        s.pre_open_baseline_c =
            s.temp.temperature_c;
    }


    /*
     * Calcula o timeout adaptativo.
     *
     * O cálculo considera:
     *
     * - temperatura atual;
     * - histórico de recuperação térmica.
     *
     * O valor obtido fica congelado durante
     * esta abertura.
     */

    s.opening_timeout_s =
        adaptive_timeout_calculate(
            &s.adaptive,
            s.temp.temperature_c
        );


    ESP_LOGI(
        TAG,
        "================================"
    );

    ESP_LOGI(
        TAG,
        "PORTA ABERTA"
    );

    ESP_LOGI(
        TAG,
        "Temperatura atual: %.2f C",
        s.temp.temperature_c
    );

    ESP_LOGI(
        TAG,
        "Baseline anterior: %.2f C",
        s.pre_open_baseline_c
    );

    ESP_LOGI(
        TAG,
        "Timeout adaptativo: %.1f segundos",
        s.opening_timeout_s
    );

    ESP_LOGI(
        TAG,
        "================================"
    );
}


/* =========================================================
 * EVENTO: PORTA FECHOU
 * ========================================================= */

static void on_door_closed(int64_t now_us)
{
    /*
     * Calcula quanto tempo a porta permaneceu aberta.
     */

    if (s.door_timer_active)
    {
        const float open_time_s =
            (float)(
                now_us -
                s.door_opened_us
            ) / 1000000.0f;


        ESP_LOGI(
            TAG,
            "PORTA FECHADA"
        );

        ESP_LOGI(
            TAG,
            "Tempo total aberta: %.1f segundos",
            open_time_s
        );
    }


    /*
     * Encerra o temporizador da porta.
     */

    s.door_timer_active = false;


    /*
     * A partir deste momento começamos a observar
     * quanto tempo a câmara demora para retornar
     * à temperatura existente antes da abertura.
     */

    adaptive_timeout_start_recovery(
        &s.adaptive,
        s.pre_open_baseline_c,
        now_us
    );
}


/* =========================================================
 * VERIFICAÇÃO DO TIMEOUT
 * ========================================================= */

static bool door_timeout_exceeded(int64_t now_us)
{
    /*
     * Se não existe temporização ativa,
     * não existe timeout.
     */

    if (!s.door_timer_active)
    {
        return false;
    }


    /*
     * A porta também precisa continuar aberta.
     */

    if (s.door != DOOR_STATE_OPEN)
    {
        return false;
    }


    /*
     * Calcula:
     *
     * tempo decorrido =
     *      tempo atual - instante da abertura
     */

    const float elapsed_s =
        (float)(
            now_us -
            s.door_opened_us
        ) / 1000000.0f;


    /*
     * Verifica se ultrapassamos o limite calculado
     * para esta abertura.
     */

    return elapsed_s >= s.opening_timeout_s;
}


/* =========================================================
 * CONTROLE DOS ALERTAS
 * ========================================================= */

static void update_outputs(int64_t now_us)
{
    /*
     * Verifica o timeout da porta.
     */

    const bool timeout_alarm =
        door_timeout_exceeded(now_us);


    /*
     * Uma condição crítica existe quando:
     *
     * 1. porta ultrapassou o timeout
     *
     * OU
     *
     * 2. temperatura ultrapassou o limite.
     */

    const bool critical =
        timeout_alarm ||
        s.temp_alarm;


    /*
     * PRIORIDADE 1:
     *
     * Situação crítica.
     */

    if (critical)
    {
        alarm_set_mode(
            ALARM_MODE_CRITICAL
        );
    }

    /*
     * PRIORIDADE 2:
     *
     * Porta aberta, mas ainda dentro
     * do tempo permitido.
     */

    else if (s.door == DOOR_STATE_OPEN)
    {
        alarm_set_mode(
            ALARM_MODE_DOOR_OPEN
        );
    }

    /*
     * Situação normal.
     */

    else
    {
        alarm_set_mode(
            ALARM_MODE_OFF
        );
    }


    /*
     * Atualiza comportamentos temporizados,
     * principalmente o pisca do LED.
     */

    alarm_update();
}


/* =========================================================
 * INICIALIZAÇÃO
 * ========================================================= */

esp_err_t app_controller_init(void)
{
    /*
     * Inicializa toda a estrutura com zero.
     */

    s = (olaf_state_t){0};


    /*
     * Inicialização dos módulos.
     */

    ESP_ERROR_CHECK(
        door_sensor_init()
    );

    ESP_ERROR_CHECK(
        temperature_ntc_init()
    );

    ESP_ERROR_CHECK(
        alarm_init()
    );


    /*
     * Inicializa o algoritmo adaptativo.
     */

    adaptive_timeout_init(
        &s.adaptive
    );


    /*
     * Obtém o estado inicial da porta.
     */

    s.door =
        door_sensor_get_state();

    s.previous_door =
        s.door;


    ESP_LOGI(
        TAG,
        "Controlador OLAF inicializado"
    );


    return ESP_OK;
}


/* =========================================================
 * TAREFA PRINCIPAL
 * ========================================================= */

void app_controller_task(void *arg)
{
    (void)arg;


    while (true)
    {
        /*
         * Tempo atual do sistema.
         *
         * esp_timer_get_time() retorna microssegundos.
         */

        const int64_t now_us =
            esp_timer_get_time();


        /* =================================================
         * 1. SENSOR DA PORTA
         * ================================================= */

        s.door =
            door_sensor_update();


        /*
         * Detectamos uma mudança real de estado.
         */

        if (s.door != s.previous_door)
        {
            /*
             * FECHADA -> ABERTA
             */

            if (s.door == DOOR_STATE_OPEN)
            {
                on_door_opened(
                    now_us
                );
            }

            /*
             * ABERTA -> FECHADA
             */

            else
            {
                on_door_closed(
                    now_us
                );
            }


            /*
             * Atualiza estado anterior.
             */

            s.previous_door =
                s.door;
        }


        /* =================================================
         * 2. TEMPERATURA
         * ================================================= */

        const int64_t temp_period_us =
            (int64_t)
            OLAF_TEMP_READ_PERIOD_MS
            * 1000LL;


        /*
         * Verifica se chegou o momento
         * de realizar uma nova leitura.
         */

        if (
            (now_us - s.last_temp_read_us)
            >= temp_period_us
        )
        {
            s.last_temp_read_us =
                now_us;


            temperature_reading_t reading =
                {0};


            /*
             * Solicita uma nova leitura ao
             * módulo do NTC.
             */

            esp_err_t temp_result =
                temperature_ntc_read(
                    &reading
                );


            if (temp_result == ESP_OK)
            {
                /*
                 * Guarda leitura.
                 */

                s.temp = reading;


                /*
                 * Atualiza o estado do
                 * alarme térmico.
                 */

                s.temp_alarm =
                    update_temperature_alarm(
                        s.temp_alarm,
                        s.temp.temperature_c
                    );


                /*
                 * =================================================
                 * APRENDIZADO DA TEMPERATURA NORMAL
                 * =================================================
                 *
                 * O baseline somente será atualizado
                 * quando:
                 *
                 * - porta estiver fechada;
                 * - não existir alarme térmico;
                 * - temperatura estiver dentro da faixa normal.
                 */

                adaptive_timeout_update_baseline(
                    &s.adaptive,
                    s.temp.temperature_c,
                    s.door == DOOR_STATE_CLOSED,
                    s.temp_alarm
                );


                /*
                 * =================================================
                 * RECUPERAÇÃO TÉRMICA
                 * =================================================
                 *
                 * Depois que a porta fecha,
                 * verificamos se a temperatura está
                 * retornando ao baseline anterior.
                 */

                if (
                    s.door ==
                    DOOR_STATE_CLOSED
                )
                {
                    adaptive_timeout_update_recovery(
                        &s.adaptive,
                        s.temp.temperature_c,
                        now_us
                    );
                }


                /*
                 * Tempo atual de abertura,
                 * utilizado principalmente no log.
                 */

                float elapsed_s = 0.0f;


                if (s.door_timer_active)
                {
                    elapsed_s =
                        (float)(
                            now_us -
                            s.door_opened_us
                        ) / 1000000.0f;
                }


                /*
                 * =================================================
                 * LOG DE MONITORAMENTO
                 * =================================================
                 */

                ESP_LOGI(
                    TAG,

                    "T=%.2f C | "
                    /*"V=%.0f mV | "
                    "R=%.0f ohm | "*/
                    "porta=%s | "
                    "tempo=%.1f s | "
                    "limite=%.1f s | "
                    "baseline=%.2f C | "
                    "recoveryEMA=%.1f s | "
                    "alarmeTemp=%d",

                    s.temp.temperature_c,

                    //s.temp.voltage_mv,

                    //s.temp.resistance_ohm,

                    (
                        s.door ==
                        DOOR_STATE_OPEN
                    )
                        ? "ABERTA"
                        : "FECHADA",

                    elapsed_s,

                    s.opening_timeout_s,

                    s.adaptive.baseline_valid
                        ? s.adaptive.baseline_temp_c
                        : NAN,

                    s.adaptive.recovery_valid
                        ? s.adaptive.recovery_ema_s
                        : NAN,

                    s.temp_alarm
                );
            }

            else
            {
                /*
                 * Falha na leitura do sensor.
                 *
                 * Como estratégia fail-safe,
                 * ativamos a condição de
                 * alarme térmico.
                 */

                ESP_LOGW(
                    TAG,
                    "Leitura invalida do NTC"
                );

                s.temp_alarm = true;
            }
        }


        /* =================================================
         * 3. LED E BUZZER
         * ================================================= */

        /*
         * Essa parte é executada localmente.
         *
         * Portanto, mesmo quando futuramente
         * adicionarmos Wi-Fi e MQTT, os alertas
         * continuarão funcionando sem internet.
         */

        update_outputs(
            now_us
        );


        /* =================================================
         * 4. AGUARDA PRÓXIMO CICLO
         * ================================================= */

        vTaskDelay(
            pdMS_TO_TICKS(
                OLAF_APP_PERIOD_MS
            )
        );
    }
}