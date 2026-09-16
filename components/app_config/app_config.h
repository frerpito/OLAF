#pragma once

#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"


/* =========================================================
 * SENSOR DA PORTA
 * =========================================================
 */

#define OLAF_DOOR_GPIO \
    ((gpio_num_t)CONFIG_OLAF_DOOR_GPIO)

#define OLAF_DOOR_CLOSED_LEVEL \
    CONFIG_OLAF_DOOR_CLOSED_LEVEL

#define OLAF_DOOR_DEBOUNCE_MS \
    CONFIG_OLAF_DOOR_DEBOUNCE_MS


/* =========================================================
 * LEDS E BUZZER
 * =========================================================
 */

#define OLAF_DOOR_LED_GPIO \
    ((gpio_num_t)CONFIG_OLAF_DOOR_LED_GPIO)

#define OLAF_TEMP_LED_GPIO \
    ((gpio_num_t)CONFIG_OLAF_TEMP_LED_GPIO)

#define OLAF_RECOVERY_LED_GPIO \
    ((gpio_num_t)CONFIG_OLAF_RECOVERY_LED_GPIO)

#define OLAF_BUZZER_GPIO \
    ((gpio_num_t)CONFIG_OLAF_BUZZER_GPIO)


/* =========================================================
 * ADC - SENSOR NTC
 * =========================================================
 */

#if CONFIG_OLAF_NTC_ADC_UNIT_1

#define OLAF_NTC_ADC_UNIT ADC_UNIT_1

#elif CONFIG_OLAF_NTC_ADC_UNIT_2

#define OLAF_NTC_ADC_UNIT ADC_UNIT_2

#endif


#if CONFIG_OLAF_NTC_ADC_CHANNEL_0
#define OLAF_NTC_ADC_CHANNEL ADC_CHANNEL_0
#elif CONFIG_OLAF_NTC_ADC_CHANNEL_1
#define OLAF_NTC_ADC_CHANNEL ADC_CHANNEL_1
#elif CONFIG_OLAF_NTC_ADC_CHANNEL_2
#define OLAF_NTC_ADC_CHANNEL ADC_CHANNEL_2
#elif CONFIG_OLAF_NTC_ADC_CHANNEL_3
#define OLAF_NTC_ADC_CHANNEL ADC_CHANNEL_3
#elif CONFIG_OLAF_NTC_ADC_CHANNEL_4
#define OLAF_NTC_ADC_CHANNEL ADC_CHANNEL_4
#elif CONFIG_OLAF_NTC_ADC_CHANNEL_5
#define OLAF_NTC_ADC_CHANNEL ADC_CHANNEL_5
#elif CONFIG_OLAF_NTC_ADC_CHANNEL_6
#define OLAF_NTC_ADC_CHANNEL ADC_CHANNEL_6
#elif CONFIG_OLAF_NTC_ADC_CHANNEL_7
#define OLAF_NTC_ADC_CHANNEL ADC_CHANNEL_7
#elif CONFIG_OLAF_NTC_ADC_CHANNEL_8
#define OLAF_NTC_ADC_CHANNEL ADC_CHANNEL_8
#elif CONFIG_OLAF_NTC_ADC_CHANNEL_9
#define OLAF_NTC_ADC_CHANNEL ADC_CHANNEL_9
#endif


#if CONFIG_OLAF_NTC_ADC_ATTEN_0

#define OLAF_NTC_ADC_ATTEN ADC_ATTEN_DB_0

#elif CONFIG_OLAF_NTC_ADC_ATTEN_2_5

#define OLAF_NTC_ADC_ATTEN ADC_ATTEN_DB_2_5

#elif CONFIG_OLAF_NTC_ADC_ATTEN_6

#define OLAF_NTC_ADC_ATTEN ADC_ATTEN_DB_6

#elif CONFIG_OLAF_NTC_ADC_ATTEN_12

#define OLAF_NTC_ADC_ATTEN ADC_ATTEN_DB_12

#endif


/* =========================================================
 * TERMISTOR NTC
 * =========================================================
 */

#define OLAF_NTC_R0_OHM \
    ((float)CONFIG_OLAF_NTC_R0_OHM)

#define OLAF_NTC_T0_C \
    ((float)CONFIG_OLAF_NTC_T0_C)

#define OLAF_NTC_BETA \
    ((float)CONFIG_OLAF_NTC_BETA)

#define OLAF_NTC_FIXED_R_OHM \
    ((float)CONFIG_OLAF_NTC_FIXED_R_OHM)

#define OLAF_ADC_SUPPLY_MV \
    ((float)CONFIG_OLAF_ADC_SUPPLY_MV)

#define OLAF_TEMP_ADC_SAMPLES \
    CONFIG_OLAF_TEMP_ADC_SAMPLES


/* =========================================================
 * INTERVALOS DA APLICAÇÃO
 * =========================================================
 */

#define OLAF_APP_PERIOD_MS \
    CONFIG_OLAF_APP_PERIOD_MS

#define OLAF_TEMP_READ_PERIOD_MS \
    CONFIG_OLAF_TEMP_READ_PERIOD_MS


/* =========================================================
 * TEMPERATURA
 * =========================================================
 */

#define OLAF_TEMP_NORMAL_MIN_C \
    ((float)CONFIG_OLAF_TEMP_NORMAL_MIN_C)

#define OLAF_TEMP_NORMAL_MAX_C \
    ((float)CONFIG_OLAF_TEMP_NORMAL_MAX_C)

#define OLAF_TEMP_ALARM_HIGH_C \
    ((float)CONFIG_OLAF_TEMP_ALARM_HIGH_C)

#define OLAF_TEMP_ALARM_CLEAR_C \
    ((float)CONFIG_OLAF_TEMP_ALARM_CLEAR_C)


/* =========================================================
 * TIMEOUT ADAPTATIVO
 * =========================================================
 */

#define OLAF_TIMEOUT_BASE_S \
    ((float)CONFIG_OLAF_TIMEOUT_BASE_S)

#define OLAF_TIMEOUT_MIN_S \
    ((float)CONFIG_OLAF_TIMEOUT_MIN_S)

#define OLAF_TIMEOUT_MAX_S \
    ((float)CONFIG_OLAF_TIMEOUT_MAX_S)


/* =========================================================
 * RECUPERAÇÃO TÉRMICA
 * =========================================================
 */

#define OLAF_RECOVERY_TARGET_S \
    ((float)CONFIG_OLAF_RECOVERY_TARGET_S)

#define OLAF_RECOVERY_STABLE_S \
    ((float)CONFIG_OLAF_RECOVERY_STABLE_S)


/* =========================================================
 * MÉDIAS MÓVEIS
 * =========================================================
 */

#define OLAF_BASELINE_ALPHA \
    ((float)CONFIG_OLAF_BASELINE_ALPHA)

#define OLAF_RECOVERY_ALPHA \
    ((float)CONFIG_OLAF_RECOVERY_ALPHA)


/* =========================================================
 * LED
 * =========================================================
 */

#define OLAF_LED_BLINK_PERIOD_MS \
    CONFIG_OLAF_LED_BLINK_PERIOD_MS


/* =========================================================
 * BUZZER
 * =========================================================
 */

#define OLAF_BUZZER_SHORT_BEEP_MS \
    CONFIG_OLAF_BUZZER_SHORT_BEEP_MS

#define OLAF_BUZZER_SHORT_PAUSE_MS \
    CONFIG_OLAF_BUZZER_SHORT_PAUSE_MS

#define OLAF_BUZZER_ALERT_ON_MS \
    CONFIG_OLAF_BUZZER_ALERT_ON_MS

#define OLAF_BUZZER_ALERT_OFF_MS \
    CONFIG_OLAF_BUZZER_ALERT_OFF_MS