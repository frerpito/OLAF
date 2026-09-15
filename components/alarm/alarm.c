#include "alarm.h"
#include "app_config.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"


static const char *TAG = "ALARM";


typedef enum
{
    BUZZER_SOURCE_NONE = 0,
    BUZZER_SOURCE_DOOR_BEEP,
    BUZZER_SOURCE_DOOR_ALERT,
    BUZZER_SOURCE_TEMP_ALERT
} buzzer_source_t;


static alarm_status_t s_status;

static bool s_door_led_on = false;
static bool s_temp_led_on = false;
static bool s_recovery_led_on = false;
static bool s_buzzer_on = false;

static int64_t s_last_door_led_toggle_us = 0;
static int64_t s_last_temp_led_toggle_us = 0;

static int s_door_beep_step = 0;
static int64_t s_door_beep_step_started_us = 0;

static bool s_temperature_alarm_previous = false;
static int64_t s_temperature_cycle_started_us = 0;

static buzzer_source_t s_active_buzzer_source = BUZZER_SOURCE_NONE;


static int64_t ms_to_us(int ms)
{
    return (int64_t)ms * 1000LL;
}


static int64_t seconds_to_us(float seconds)
{
    return (int64_t)(seconds * 1000000.0f);
}


static float valid_recovery_time_s(float recovery_time_s)
{
    if (isfinite(recovery_time_s) && recovery_time_s > 0.0f)
    {
        return recovery_time_s;
    }

    return OLAF_RECOVERY_TARGET_S;
}


static esp_err_t configure_output(gpio_num_t gpio, const char *name)
{
    gpio_config_t config =
    {
        .pin_bit_mask = (1ULL << gpio),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    esp_err_t err = gpio_config(&config);

    if (err != ESP_OK)
    {
        ESP_LOGE(
            TAG,
            "Erro ao configurar %s no GPIO %d: %s",
            name,
            gpio,
            esp_err_to_name(err)
        );
    }

    return err;
}


static void set_gpio_state(gpio_num_t gpio, bool *cached_state, bool enabled)
{
    if (*cached_state == enabled)
    {
        return;
    }

    gpio_set_level(gpio, enabled ? 1 : 0);
    *cached_state = enabled;
}


static void set_door_led(bool enabled)
{
    set_gpio_state(
        OLAF_DOOR_LED_GPIO,
        &s_door_led_on,
        enabled
    );
}


static void set_temp_led(bool enabled)
{
    set_gpio_state(
        OLAF_TEMP_LED_GPIO,
        &s_temp_led_on,
        enabled
    );
}


static void set_recovery_led(bool enabled)
{
    set_gpio_state(
        OLAF_RECOVERY_LED_GPIO,
        &s_recovery_led_on,
        enabled
    );
}


static void set_buzzer(bool enabled)
{
    set_gpio_state(
        OLAF_BUZZER_GPIO,
        &s_buzzer_on,
        enabled
    );
}


static void update_door_led(int64_t now_us)
{
    if (!s_status.door_open)
    {
        set_door_led(false);
        return;
    }

    if (!s_status.door_timeout_alarm)
    {
        set_door_led(true);
        return;
    }

    if ((now_us - s_last_door_led_toggle_us) >=
        ms_to_us(OLAF_LED_BLINK_PERIOD_MS))
    {
        set_door_led(!s_door_led_on);
        s_last_door_led_toggle_us = now_us;
    }
}


static void update_temp_led(int64_t now_us)
{
    if (!s_status.temperature_warning)
    {
        set_temp_led(true);
        return;
    }

    if ((now_us - s_last_temp_led_toggle_us) >=
        ms_to_us(OLAF_LED_BLINK_PERIOD_MS))
    {
        set_temp_led(!s_temp_led_on);
        s_last_temp_led_toggle_us = now_us;
    }
}


static void update_recovery_led(void)
{
    set_recovery_led(s_status.recovery_active);
}


static bool update_door_beep(int64_t now_us)
{
    if (s_door_beep_step <= 0)
    {
        return false;
    }

    const int64_t elapsed_us =
        now_us - s_door_beep_step_started_us;

    switch (s_door_beep_step)
    {
    case 1:
    case 3:
        if (elapsed_us >= ms_to_us(OLAF_BUZZER_SHORT_BEEP_MS))
        {
            s_door_beep_step++;
            s_door_beep_step_started_us = now_us;
        }
        break;

    case 2:
        if (elapsed_us >= ms_to_us(OLAF_BUZZER_SHORT_PAUSE_MS))
        {
            s_door_beep_step++;
            s_door_beep_step_started_us = now_us;
        }
        break;

    default:
        s_door_beep_step = 0;
        return false;
    }

    return s_door_beep_step == 1 || s_door_beep_step == 3;
}


static bool door_alert_buzzer_on(int64_t now_us)
{
    const int64_t cycle_us =
        ms_to_us(OLAF_BUZZER_ALERT_ON_MS + OLAF_BUZZER_ALERT_OFF_MS);

    const int64_t position_us =
        now_us % cycle_us;

    return position_us < ms_to_us(OLAF_BUZZER_ALERT_ON_MS);
}


static bool temp_alert_buzzer_on(int64_t now_us)
{
    if (!s_status.temperature_alarm)
    {
        return false;
    }

    const float recovery_time_s =
        valid_recovery_time_s(s_status.recovery_time_s);

    const int64_t recovery_us =
        seconds_to_us(recovery_time_s);

    const int64_t elapsed_us =
        now_us - s_temperature_cycle_started_us;

    if (elapsed_us < 0)
    {
        return false;
    }

    const int64_t cycle_us =
        recovery_us * 2;

    if (cycle_us <= 0)
    {
        return false;
    }

    const int64_t position_us =
        elapsed_us % cycle_us;

    if (position_us >= recovery_us)
    {
        return false;
    }

    const int64_t beep_cycle_us =
        ms_to_us(OLAF_BUZZER_ALERT_ON_MS + OLAF_BUZZER_ALERT_OFF_MS);

    const int64_t beep_position_us =
        position_us % beep_cycle_us;

    return beep_position_us < ms_to_us(OLAF_BUZZER_ALERT_ON_MS);
}


static void update_buzzer(int64_t now_us)
{
    bool enabled = false;
    buzzer_source_t source = BUZZER_SOURCE_NONE;

    if (s_status.door_timeout_alarm)
    {
        enabled = door_alert_buzzer_on(now_us);
        source = BUZZER_SOURCE_DOOR_ALERT;
    }
    else if (s_status.temperature_alarm)
    {
        enabled = temp_alert_buzzer_on(now_us);
        source = BUZZER_SOURCE_TEMP_ALERT;
    }
    else if (s_door_beep_step > 0)
    {
        enabled = update_door_beep(now_us);
        source = BUZZER_SOURCE_DOOR_BEEP;
    }
    else
    {
        (void)update_door_beep(now_us);
    }

    if (source != s_active_buzzer_source)
    {
        s_active_buzzer_source = source;
        ESP_LOGI(TAG, "Fonte do buzzer: %d", source);
    }

    set_buzzer(enabled);
}


esp_err_t alarm_init(void)
{
    esp_err_t err =
        configure_output(
            OLAF_DOOR_LED_GPIO,
            "LED da porta"
        );

    if (err != ESP_OK)
    {
        return err;
    }

    err =
        configure_output(
            OLAF_TEMP_LED_GPIO,
            "LED de temperatura"
        );

    if (err != ESP_OK)
    {
        return err;
    }

    err =
        configure_output(
            OLAF_RECOVERY_LED_GPIO,
            "LED de recuperacao"
        );

    if (err != ESP_OK)
    {
        return err;
    }

    err =
        configure_output(
            OLAF_BUZZER_GPIO,
            "buzzer"
        );

    if (err != ESP_OK)
    {
        return err;
    }

    s_status = (alarm_status_t){0};

    set_door_led(false);
    set_temp_led(true);
    set_recovery_led(false);
    set_buzzer(false);

    int64_t now_us = esp_timer_get_time();

    s_last_door_led_toggle_us = now_us;
    s_last_temp_led_toggle_us = now_us;
    s_temperature_cycle_started_us = now_us;

    ESP_LOGI(
        TAG,
        "Alarme inicializado | porta LED GPIO %d | temp LED GPIO %d | recovery LED GPIO %d | buzzer GPIO %d",
        OLAF_DOOR_LED_GPIO,
        OLAF_TEMP_LED_GPIO,
        OLAF_RECOVERY_LED_GPIO,
        OLAF_BUZZER_GPIO
    );

    return ESP_OK;
}


void alarm_notify_door_changed(void)
{
    s_door_beep_step = 1;
    s_door_beep_step_started_us = esp_timer_get_time();
}


void alarm_set_status(const alarm_status_t *status)
{
    if (status == NULL)
    {
        return;
    }

    const bool temperature_alarm_started =
        status->temperature_alarm &&
        !s_temperature_alarm_previous;

    s_status = *status;
    s_status.recovery_time_s =
        valid_recovery_time_s(status->recovery_time_s);

    if (temperature_alarm_started)
    {
        s_temperature_cycle_started_us = esp_timer_get_time();
    }

    s_temperature_alarm_previous =
        s_status.temperature_alarm;
}


void alarm_update(void)
{
    const int64_t now_us =
        esp_timer_get_time();

    update_door_led(now_us);
    update_temp_led(now_us);
    update_recovery_led();
    update_buzzer(now_us);
}
