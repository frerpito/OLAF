#include "alarm.h"
#include "app_config.h"

#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"


static const char *TAG = "ALARM";


/* =========================================================
 * ESTADO INTERNO
 * ========================================================= */

/*
 * Modo atual do sistema de sinalização.
 */
static alarm_mode_t s_mode = ALARM_MODE_OFF;


/*
 * Estado atual do LED.
 *
 * Utilizado principalmente durante o modo
 * de pisca do alerta crítico.
 */
static bool s_led_state = false;


/*
 * Instante da última mudança de estado do LED.
 */
static int64_t s_last_blink_us = 0;


/* =========================================================
 * FUNÇÕES INTERNAS
 * ========================================================= */

/**
 * @brief Altera fisicamente o estado do LED.
 */
static void set_led(bool enabled)
{
    gpio_set_level(
        OLAF_LED_GPIO,
        enabled ? 1 : 0
    );

    s_led_state = enabled;
}


/**
 * @brief Altera fisicamente o estado do buzzer.
 *
 * O código considera um buzzer ATIVO.
 *
 * Portanto:
 *
 * HIGH -> buzzer ligado
 * LOW  -> buzzer desligado
 */
static void set_buzzer(bool enabled)
{
    gpio_set_level(
        OLAF_BUZZER_GPIO,
        enabled ? 1 : 0
    );
}


/* =========================================================
 * INICIALIZAÇÃO
 * ========================================================= */

esp_err_t alarm_init(void)
{
    /* =====================================================
     * CONFIGURA LED
     * ===================================================== */

    gpio_config_t led_config =
    {
        .pin_bit_mask =
            (1ULL << OLAF_LED_GPIO),

        .mode =
            GPIO_MODE_OUTPUT,

        .pull_up_en =
            GPIO_PULLUP_DISABLE,

        .pull_down_en =
            GPIO_PULLDOWN_DISABLE,

        .intr_type =
            GPIO_INTR_DISABLE
    };


    esp_err_t err =
        gpio_config(
            &led_config
        );


    if (err != ESP_OK)
    {
        ESP_LOGE(
            TAG,
            "Erro ao configurar LED: %s",
            esp_err_to_name(err)
        );

        return err;
    }


    /* =====================================================
     * CONFIGURA BUZZER
     * ===================================================== */

    gpio_config_t buzzer_config =
    {
        .pin_bit_mask =
            (1ULL << OLAF_BUZZER_GPIO),

        .mode =
            GPIO_MODE_OUTPUT,

        .pull_up_en =
            GPIO_PULLUP_DISABLE,

        .pull_down_en =
            GPIO_PULLDOWN_DISABLE,

        .intr_type =
            GPIO_INTR_DISABLE
    };


    err =
        gpio_config(
            &buzzer_config
        );


    if (err != ESP_OK)
    {
        ESP_LOGE(
            TAG,
            "Erro ao configurar buzzer: %s",
            esp_err_to_name(err)
        );

        return err;
    }


    /* =====================================================
     * ESTADO INICIAL
     * ===================================================== */

    set_led(false);
    set_buzzer(false);

    s_mode =
        ALARM_MODE_OFF;

    s_last_blink_us =
        esp_timer_get_time();


    ESP_LOGI(
        TAG,
        "Sistema de alarme inicializado"
    );


    return ESP_OK;
}


/* =========================================================
 * ALTERAÇÃO DO MODO
 * ========================================================= */

void alarm_set_mode(alarm_mode_t mode)
{
    /*
     * Se já estamos nesse modo, não precisamos
     * reinicializar as saídas.
     */
    if (mode == s_mode)
    {
        return;
    }


    /*
     * Guarda o novo modo.
     */
    s_mode = mode;


    /* =====================================================
     * NORMAL
     * ===================================================== */

    if (s_mode == ALARM_MODE_OFF)
    {
        /*
         * Nenhuma condição de alerta.
         */

        set_led(false);
        set_buzzer(false);


        ESP_LOGI(
            TAG,
            "Alarme: NORMAL"
        );
    }


    /* =====================================================
     * PORTA ABERTA
     * ===================================================== */

    else if (
        s_mode ==
        ALARM_MODE_DOOR_OPEN
    )
    {
        /*
         * Porta aberta, mas ainda dentro
         * do timeout permitido.
         *
         * LED fica continuamente aceso.
         * Buzzer permanece desligado.
         */

        set_led(true);
        set_buzzer(false);


        ESP_LOGI(
            TAG,
            "Alarme: PORTA ABERTA"
        );
    }


    /* =====================================================
     * CRÍTICO
     * ===================================================== */

    else if (
        s_mode ==
        ALARM_MODE_CRITICAL
    )
    {
        /*
         * Situação crítica:
         *
         * - timeout da porta;
         *
         * OU
         *
         * - temperatura acima do limite.
         *
         * O buzzer é ativado imediatamente.
         */

        set_buzzer(true);


        /*
         * Reiniciamos o controle do pisca.
         *
         * Começamos com LED ligado para que
         * o alerta visual seja imediato.
         */

        set_led(true);

        s_last_blink_us =
            esp_timer_get_time();


        ESP_LOGW(
            TAG,
            "Alarme: CRITICO"
        );
    }
}


/* =========================================================
 * ATUALIZAÇÃO
 * ========================================================= */

void alarm_update(void)
{
    /*
     * O pisca somente é necessário durante
     * uma condição crítica.
     */

    if (s_mode != ALARM_MODE_CRITICAL)
    {
        return;
    }


    /*
     * Obtém o tempo atual.
     */

    const int64_t now_us =
        esp_timer_get_time();


    /*
     * Converte o período de milissegundos
     * para microssegundos.
     */

    const int64_t blink_period_us =
        (int64_t)
        OLAF_LED_BLINK_PERIOD_MS
        * 1000LL;


    /*
     * Verifica quanto tempo passou desde
     * a última mudança.
     */

    const int64_t elapsed_us =
        now_us -
        s_last_blink_us;


    /*
     * Se atingimos o período configurado,
     * invertemos o LED.
     */

    if (elapsed_us >= blink_period_us)
    {
        s_led_state =
            !s_led_state;


        gpio_set_level(
            OLAF_LED_GPIO,
            s_led_state ? 1 : 0
        );


        /*
         * Atualiza a referência temporal.
         */

        s_last_blink_us =
            now_us;
    }
}


/* =========================================================
 * CONSULTA DO MODO
 * ========================================================= */

alarm_mode_t alarm_get_mode(void)
{
    return s_mode;
}