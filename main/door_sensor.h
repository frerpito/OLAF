#pragma once

#include <stdbool.h>

#include "esp_err.h"


/* =========================================================
 * ESTADOS POSSÍVEIS DA PORTA
 * ========================================================= */

/**
 * @brief Representa o estado físico da porta da
 *        câmara frigorífica.
 */
typedef enum
{
    /*
     * O sensor E18-D80NK está detectando a chapa
     * instalada na porta.
     */
    DOOR_STATE_CLOSED = 0,

    /*
     * O sensor deixou de detectar a chapa,
     * indicando abertura da porta.
     */
    DOOR_STATE_OPEN

} door_state_t;


/* =========================================================
 * FUNÇÕES PÚBLICAS
 * ========================================================= */

/**
 * @brief Inicializa o sensor responsável pela detecção
 *        do estado da porta.
 *
 * Configura o GPIO utilizado pelo E18-D80NK e realiza
 * uma leitura inicial para determinar se a porta está
 * aberta ou fechada.
 *
 * @return
 *      ESP_OK em caso de sucesso.
 *      Código de erro ESP-IDF em caso de falha.
 */
esp_err_t door_sensor_init(void);


/**
 * @brief Atualiza e retorna o estado da porta.
 *
 * Realiza a leitura do GPIO e aplica o filtro temporal
 * configurado por OLAF_DOOR_DEBOUNCE_MS.
 *
 * Uma mudança somente é considerada válida caso o
 * sinal permaneça estável durante o período definido.
 *
 * @return
 *      DOOR_STATE_OPEN
 *      DOOR_STATE_CLOSED
 */
door_state_t door_sensor_update(void);


/**
 * @brief Retorna o último estado válido conhecido
 *        da porta.
 *
 * Diferentemente de door_sensor_update(), esta função
 * não realiza uma nova leitura do GPIO.
 *
 * @return Estado atual armazenado da porta.
 */
door_state_t door_sensor_get_state(void);