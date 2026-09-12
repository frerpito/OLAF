#pragma once

#include <stdbool.h>

#include "esp_err.h"


/* =========================================================
 * ESTRUTURA DA LEITURA DE TEMPERATURA
 * ========================================================= */

/**
 * @brief Representa o resultado completo de uma leitura
 *        realizada pelo sensor NTC.
 *
 * Além da temperatura, mantemos a tensão e a resistência
 * calculadas para facilitar:
 *
 * - depuração;
 * - calibração;
 * - diagnóstico do sensor;
 * - visualização futura via MQTT/dashboard.
 */
typedef struct
{
    /**
     * @brief Tensão média medida no divisor resistivo.
     *
     * Unidade: milivolts (mV).
     *
     * Exemplo:
     * 1650.0 mV
     */
    float voltage_mv;


    /**
     * @brief Resistência calculada do termistor NTC.
     *
     * Unidade: ohms.
     *
     * Exemplo:
     * 10000.0 ohms
     */
    float resistance_ohm;


    /**
     * @brief Temperatura calculada.
     *
     * Unidade: graus Celsius.
     *
     * Exemplo:
     * 3.25 °C
     */
    float temperature_c;


    /**
     * @brief Indica se a leitura pode ser considerada válida.
     *
     * true:
     *     leitura válida.
     *
     * false:
     *     leitura inválida ou fora da faixa esperada.
     */
    bool valid;

} temperature_reading_t;


/* =========================================================
 * FUNÇÕES PÚBLICAS
 * ========================================================= */

/**
 * @brief Inicializa o ADC e o sistema de calibração
 *        utilizado pelo NTC.
 *
 * Essa função deve ser chamada uma única vez durante
 * a inicialização do sistema.
 *
 * Internamente realiza:
 *
 * 1. criação da unidade ADC;
 * 2. configuração do canal;
 * 3. configuração da atenuação;
 * 4. configuração da resolução;
 * 5. tentativa de inicialização da calibração.
 *
 * @return
 *      ESP_OK em caso de sucesso.
 *
 *      Código de erro ESP-IDF em caso de falha.
 */
esp_err_t temperature_ntc_init(void);


/**
 * @brief Realiza uma nova medição de temperatura.
 *
 * O processo executado é:
 *
 * ADC
 *  ↓
 * múltiplas amostras
 *  ↓
 * média
 *  ↓
 * tensão
 *  ↓
 * resistência
 *  ↓
 * equação Beta
 *  ↓
 * temperatura em °C
 *
 * A quantidade de amostras utilizada é definida por:
 *
 * OLAF_TEMP_ADC_SAMPLES
 *
 * em app_config.h.
 *
 * @param out Ponteiro para estrutura que receberá
 *            o resultado da medição.
 *
 * @return
 *      ESP_OK
 *          Leitura realizada com sucesso.
 *
 *      ESP_ERR_INVALID_ARG
 *          Ponteiro de saída inválido.
 *
 *      ESP_ERR_INVALID_STATE
 *          ADC ainda não foi inicializado.
 *
 *      ESP_ERR_INVALID_RESPONSE
 *          Valor calculado considerado inválido.
 *
 *      Outros códigos
 *          Erros provenientes do driver ADC.
 */
esp_err_t temperature_ntc_read(
    temperature_reading_t *out
);