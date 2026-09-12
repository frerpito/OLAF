#pragma once

#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"


/* Sensor de porta E18-D80NK */
#define OLAF_DOOR_GPIO              GPIO_NUM_5

/* LED de sinalização */
#define OLAF_LED_GPIO               GPIO_NUM_35

/* Buzzer piezoelétrico ativo */
#define OLAF_BUZZER_GPIO            GPIO_NUM_36


/* =========================================================
 * SENSOR DA PORTA
 * =========================================================
 *
 * Define qual nível lógico representa a porta fechada.
 *
 * Quando a chapa metálica estiver sendo detectada pelo
 * E18-D80NK, consideramos a porta fechada.
 *
 * Dependendo da interface elétrica utilizada com o sensor,
 * talvez seja necessário trocar 1 por 0.
 */

#define OLAF_DOOR_CLOSED_LEVEL      0


/*
 * Tempo durante o qual o sinal deve permanecer estável
 * antes de uma mudança de estado ser aceita.
 *
 * Isso ajuda a evitar falsos eventos provocados por
 * oscilações rápidas do sensor.
 */

#define OLAF_DOOR_DEBOUNCE_MS       80


/* =========================================================
 * ADC - SENSOR NTC
 * ========================================================= */

/*
 * Utilizamos ADC1.
 *
 * O canal precisa ser conferido com o GPIO físico utilizado
 * na placa ESP32-S3.
 */

#define OLAF_NTC_ADC_UNIT           ADC_UNIT_1

#define OLAF_NTC_ADC_CHANNEL        ADC_CHANNEL_3

#define OLAF_NTC_ADC_ATTEN          ADC_ATTEN_DB_12


/* =========================================================
 * TERMISTOR NTC
 * =========================================================
 *
 * Valores iniciais para um NTC 10 kΩ.
 *
 * O valor BETA deve ser confirmado no datasheet do
 * componente MF52 utilizado no projeto.
 */

/* Resistência nominal do NTC em T0 */
#define OLAF_NTC_R0_OHM             10000.0f

/* Temperatura de referência */
#define OLAF_NTC_T0_C               25.0f

/* Constante Beta inicial */
#define OLAF_NTC_BETA               3950.0f


/* =========================================================
 * DIVISOR DE TENSÃO DO NTC
 * =========================================================
 *
 *
 *          3.3 V
 *            |
 *         R_FIXED
 *          10 kΩ
 *            |
 *            +---------- ADC
 *            |
 *           NTC
 *            |
 *           GND
 *
 */

#define OLAF_NTC_FIXED_R_OHM        10000.0f

#define OLAF_ADC_SUPPLY_MV          3300.0f


/* =========================================================
 * FILTRO / MÉDIA DO ADC
 * =========================================================
 *
 * Em cada medição de temperatura são realizadas várias
 * leituras ADC.
 *
 * Depois é calculada a média dessas amostras.
 *
 * Isso reduz o efeito de ruído de uma leitura individual.
 */

#define OLAF_TEMP_ADC_SAMPLES       32


/* =========================================================
 * INTERVALOS DA APLICAÇÃO
 * ========================================================= */

/*
 * Intervalo da tarefa principal.
 *
 * A cada 200 ms verificamos:
 *
 * - sensor da porta;
 * - temporização;
 * - LED;
 * - buzzer;
 * - condições de alerta.
 */

#define OLAF_APP_PERIOD_MS          200


/*
 * Temperatura não precisa ser convertida a cada ciclo
 * de 200 ms.
 *
 * Realizamos uma nova medição a cada segundo.
 */

#define OLAF_TEMP_READ_PERIOD_MS    1000


/*
 *
 * Valores iniciais para os ensaios.
 *
 * Devem ser posteriormente configurados de acordo com
 * a aplicação real da câmara frigorífica.
 */

#define OLAF_TEMP_NORMAL_MIN_C      24.0f

#define OLAF_TEMP_NORMAL_MAX_C      27.0f





#define OLAF_TEMP_ALARM_HIGH_C      34.0f


/*
 * Histerese:
 *
 * Depois que o alarme for ativado em 6 °C,
 * ele somente será removido quando a temperatura
 * cair para 5,5 °C ou menos.
 *
 * Isso evita:
 *
 * 5.99 -> normal
 * 6.01 -> alarme
 * 5.99 -> normal
 * 6.01 -> alarme
 */

#define OLAF_TEMP_ALARM_CLEAR_C     28.0f


/* =========================================================
 * TIMEOUT ADAPTATIVO
 * ========================================================= */

/*
 * Timeout inicial utilizado como referência.
 *
 * 300 segundos = 5 minutos.
 */

#define OLAF_TIMEOUT_BASE_S         300.0f


/*
 * O algoritmo adaptativo nunca poderá fornecer
 * menos que 30 segundos.
 */

#define OLAF_TIMEOUT_MIN_S          30.0f


/*
 * Mesmo em condições térmicas muito favoráveis,
 * limitamos o tempo máximo a 420 segundos.
 *
 * 420 segundos = 7 minutos.
 */

#define OLAF_TIMEOUT_MAX_S          420.0f


/* =========================================================
 * RECUPERAÇÃO TÉRMICA
 * ========================================================= */

/*
 * Tempo de recuperação considerado como referência.
 *
 * Se a câmara normalmente recuperar em aproximadamente
 * 300 segundos, o fator de recuperação será próximo de 1.
 *
 * Se começar a demorar mais, o timeout das próximas
 * aberturas será reduzido. (guilherme ainda  vai ajustar isso)
 */

#define OLAF_RECOVERY_TARGET_S      300.0f


/*
 * Depois que a temperatura voltar para próximo do baseline,
 * ela deve permanecer estável durante este período antes
 * de considerarmos a recuperação concluída.
 */

#define OLAF_RECOVERY_STABLE_S      20.0f


/* =========================================================
 * MÉDIAS MÓVEIS
 * ========================================================= */

/*
 * Alpha da média móvel exponencial utilizada para
 * aprender a temperatura normal da câmara.
 *
 * Valor pequeno:
 * aprendizado mais lento e estável.
 */

#define OLAF_BASELINE_ALPHA         0.02f


/*
 * Alpha utilizado para aprender o tempo médio
 * de recuperação térmica.
 */

#define OLAF_RECOVERY_ALPHA         0.20f


/* =========================================================
 * LED
 * ========================================================= */


#define OLAF_LED_BLINK_PERIOD_MS    500