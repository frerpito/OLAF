#include "app_controller.h"

#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_timer.h"

#include "app_config.h"
#include "door_sensor.h"
#include "temperature_ntc.h"
#include "alarm.h"
#include "adaptive_timeout.h"
#include "mqtt_component.h"


static const char *TAG = "OLAF";


/* =========================================================
 * ESTADO GERAL DA APLICAÇÃO
 * ========================================================= */

typedef struct
{
    /*
     * Estado atual e anterior da porta.
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
     */
    float opening_timeout_s;

    /*
     * Temperatura de referência antes da abertura.
     */
    float pre_open_baseline_c;

    /*
     * Momento ate o qual o LED de recuperacao
     * deve permanecer aceso.
     */
    int64_t recovery_led_until_us;

    /*
     * Estrutura responsável pelo aprendizado
     * do comportamento térmico.
     */
    adaptive_timeout_t adaptive;

} olaf_state_t;


static olaf_state_t s;


/* =========================================================
 * PROTÓTIPOS INTERNOS
 * ========================================================= */

static bool temperature_above_normal(float temperature_c);

static bool temperature_recovery_active(int64_t now_us);

static void start_temperature_recovery_window(int64_t now_us);

static bool update_temperature_alarm(
    bool previous_alarm,
    float temperature_c,
    bool recovery_active
);

static void on_door_opened(int64_t now_us);

static void on_door_closed(int64_t now_us);

static bool door_timeout_exceeded(int64_t now_us);

static float estimated_recovery_time_s(void);

static void update_outputs(int64_t now_us);

static void publish_mqtt_state(float elapsed_s);

static void mqtt_command_callback(
    const char *topic,
    int topic_len,
    const char *data,
    int data_len
);

static void app_controller_task(void *arg);


/* =========================================================
 * ALARME DE TEMPERATURA
 * ========================================================= */

static bool update_temperature_alarm(
    bool previous_alarm,
    float temperature_c,
    bool recovery_active)
{
    /*
     * Leitura inválida é considerada condição anormal.
     */
    if (!isfinite(temperature_c))
    {
        return true;
    }

    if (recovery_active)
    {
        return false;
    }


    /*
     * Ativação do alarme.
     */
    if (!previous_alarm &&
        temperature_above_normal(temperature_c))
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
        !temperature_above_normal(temperature_c))
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
    alarm_notify_door_changed();

    /*
     * Inicia a contagem do tempo.
     */
    s.door_timer_active = true;

    s.door_opened_us = now_us;


    /*
     * Guarda a temperatura média de referência
     * existente antes da abertura.
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
    alarm_notify_door_changed();

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


    if (temperature_above_normal(s.temp.temperature_c))
    {
        /*
         * Inicia acompanhamento da recuperação térmica
         * somente quando existe temperatura a recuperar.
         */
        adaptive_timeout_start_recovery(
            &s.adaptive,
            s.pre_open_baseline_c,
            now_us
        );


        /*
         * LED de recuperacao: tempo estimado de recuperacao
         * mais 1 minuto de margem.
         */
        start_temperature_recovery_window(now_us);
    }
    else
    {
        s.recovery_led_until_us = 0;
        s.temp_alarm = false;
    }
}


static bool temperature_above_normal(float temperature_c)
{
    return
        isfinite(temperature_c) &&
        temperature_c > OLAF_TEMP_NORMAL_MAX_C;
}


static bool temperature_recovery_active(int64_t now_us)
{
    return
        s.recovery_led_until_us > 0 &&
        now_us < s.recovery_led_until_us;
}


static void start_temperature_recovery_window(int64_t now_us)
{
    s.recovery_led_until_us =
        now_us +
        (int64_t)(
            (estimated_recovery_time_s() + 60.0f)
            *
            1000000.0f
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


    const float elapsed_s =
        (float)(
            now_us -
            s.door_opened_us
        ) / 1000000.0f;


    return elapsed_s >= s.opening_timeout_s;
}


static float estimated_recovery_time_s(void)
{
    if (
        s.adaptive.recovery_valid &&
        isfinite(s.adaptive.recovery_ema_s) &&
        s.adaptive.recovery_ema_s > 0.0f
    )
    {
        return s.adaptive.recovery_ema_s;
    }

    return OLAF_RECOVERY_TARGET_S;
}


/* =========================================================
 * CONTROLE DOS ALERTAS
 * ========================================================= */

static void update_outputs(int64_t now_us)
{
    const bool timeout_alarm =
        door_timeout_exceeded(now_us);

    const bool recovery_led_active =
        temperature_recovery_active(now_us) &&
        temperature_above_normal(s.temp.temperature_c);

    const bool temperature_warning =
        temperature_above_normal(s.temp.temperature_c);

    alarm_status_t alarm_status =
    {
        .door_open =
            s.door == DOOR_STATE_OPEN,

        .door_timeout_alarm =
            timeout_alarm,

        .temperature_warning =
            temperature_warning,

        .temperature_alarm =
            s.temp_alarm,

        .recovery_active =
            recovery_led_active,

        .recovery_time_s =
            estimated_recovery_time_s()
    };


    alarm_set_status(
        &alarm_status
    );


    /*
     * Atualiza comportamentos temporizados
     * do alarme.
     */
    alarm_update();
}


/* =========================================================
 * MQTT
 * ========================================================= */

static void mqtt_command_callback(
    const char *topic,
    int topic_len,
    const char *data,
    int data_len)
{
    ESP_LOGI(
        TAG,
        "Comando MQTT recebido: %.*s -> %.*s",
        topic_len,
        topic,
        data_len,
        data
    );
}


static void publish_mqtt_state(float elapsed_s)
{
    static bool command_subscribed = false;

    if (!mqtt_is_connected())
    {
        command_subscribed = false;
        ESP_LOGW(TAG, "MQTT ainda nao conectado; leitura nao publicada");
        return;
    }

    if (!command_subscribed)
    {
        int msg_id = mqtt_subscribe(
            "sensor/comando",
            1
        );

        if (msg_id >= 0)
        {
            command_subscribed = true;
            ESP_LOGI(TAG, "Inscrito no topico sensor/comando");
        }
        else
        {
            ESP_LOGW(TAG, "Falha ao assinar sensor/comando");
        }
    }

    char payload[64];

    snprintf(
        payload,
        sizeof(payload),
        "%.2f",
        s.temp.temperature_c
    );

    mqtt_publish(
        "sensor/temperatura",
        payload,
        1,
        0
    );

    mqtt_publish(
        "sensor/porta",
        s.door == DOOR_STATE_OPEN ? "aberta" : "fechada",
        1,
        0
    );

    snprintf(
        payload,
        sizeof(payload),
        "%d",
        s.temp_alarm ? 1 : 0
    );

    mqtt_publish(
        "sensor/alarme",
        payload,
        1,
        0
    );

    snprintf(
        payload,
        sizeof(payload),
        "%.1f",
        elapsed_s
    );

    mqtt_publish(
        "sensor/tempo_porta",
        payload,
        1,
        0
    );
}


/* =========================================================
 * INICIALIZAÇÃO
 * ========================================================= */

esp_err_t app_controller_init(void)
{
    /*
     * Limpa o estado do controlador.
     */
    s = (olaf_state_t){0};


    esp_err_t err;


    /*
     * Inicializa sensor da porta.
     */
    err = door_sensor_init();

    if (err != ESP_OK)
    {
        ESP_LOGE(
            TAG,
            "Falha ao inicializar sensor da porta: %s",
            esp_err_to_name(err)
        );

        return err;
    }


    /*
     * Inicializa sensor de temperatura.
     */
    err = temperature_ntc_init();

    if (err != ESP_OK)
    {
        ESP_LOGE(
            TAG,
            "Falha ao inicializar sensor de temperatura: %s",
            esp_err_to_name(err)
        );

        return err;
    }


    /*
     * Inicializa sistema de alarme.
     */
    err = alarm_init();

    if (err != ESP_OK)
    {
        ESP_LOGE(
            TAG,
            "Falha ao inicializar alarme: %s",
            esp_err_to_name(err)
        );

        return err;
    }


    /*
     * Inicializa algoritmo adaptativo.
     */
    adaptive_timeout_init(
        &s.adaptive
    );


    mqtt_set_message_callback(
        mqtt_command_callback
    );


    /*
     * Obtém estado inicial da porta.
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
 * INICIALIZAÇÃO DA TAREFA
 * ========================================================= */

esp_err_t app_controller_start(void)
{
    BaseType_t task_created =
        xTaskCreate(
            app_controller_task,
            "olaf_controller",
            6144,
            NULL,
            5,
            NULL
        );


    if (task_created != pdPASS)
    {
        ESP_LOGE(
            TAG,
            "Nao foi possivel criar a tarefa principal"
        );

        return ESP_FAIL;
    }


    ESP_LOGI(
        TAG,
        "Tarefa principal criada com sucesso"
    );


    return ESP_OK;
}


/* =========================================================
 * TAREFA PRINCIPAL
 * ========================================================= */

static void app_controller_task(void *arg)
{
    (void)arg;


    while (true)
    {
        /*
         * Tempo atual do sistema.
         */
        const int64_t now_us =
            esp_timer_get_time();


        /* =================================================
         * 1. SENSOR DA PORTA
         * ================================================= */

        s.door =
            door_sensor_update();


        /*
         * Detecta mudança de estado.
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


        if (
            (now_us - s.last_temp_read_us)
            >= temp_period_us
        )
        {
            s.last_temp_read_us =
                now_us;


            temperature_reading_t reading =
                {0};


            esp_err_t temp_result =
                temperature_ntc_read(
                    &reading
                );


            if (temp_result == ESP_OK)
            {
                s.temp =
                    reading;

                const bool temperature_warning =
                    temperature_above_normal(s.temp.temperature_c);

                if (!temperature_warning)
                {
                    s.recovery_led_until_us = 0;
                    s.temp_alarm = false;
                }
                else if (
                    !temperature_recovery_active(now_us) &&
                    !s.temp_alarm
                )
                {
                    start_temperature_recovery_window(now_us);
                }

                const bool recovery_active =
                    temperature_recovery_active(now_us);


                /*
                 * Atualiza alarme térmico.
                 */
                s.temp_alarm =
                    update_temperature_alarm(
                        s.temp_alarm,
                        s.temp.temperature_c,
                        recovery_active
                    );


                /*
                 * Atualiza baseline.
                 */
                adaptive_timeout_update_baseline(
                    &s.adaptive,
                    s.temp.temperature_c,
                    s.door == DOOR_STATE_CLOSED,
                    s.temp_alarm
                );


                /*
                 * Atualiza recuperação térmica.
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


                float elapsed_s =
                    0.0f;


                if (s.door_timer_active)
                {
                    elapsed_s =
                        (float)(
                            now_us -
                            s.door_opened_us
                        ) / 1000000.0f;
                }


                /*
                 * Log do sistema.
                 */
                ESP_LOGI(
                    TAG,

                    "T=%.2f C | "
                    "porta=%s | "
                    "tempo=%.1f s | "
                    "limite=%.1f s | "
                    "baseline=%.2f C | "
                    "recoveryEMA=%.1f s | "
                    "alarmeTemp=%d",

                    s.temp.temperature_c,

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


                publish_mqtt_state(
                    elapsed_s
                );
            }

            else
            {
                /*
                 * Falha na leitura do NTC.
                 *
                 * Estratégia fail-safe:
                 * considera condição térmica anormal.
                 */
                ESP_LOGW(
                    TAG,
                    "Leitura invalida do NTC"
                );

                s.temp_alarm =
                    true;
            }
        }


        /* =================================================
         * 3. LED E BUZZER
         * ================================================= */

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
