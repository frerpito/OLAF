#pragma once

#include "esp_err.h"


/* =========================================================
 * MODOS DO SISTEMA DE ALARME
 * ========================================================= */

/**
 * @brief Estados possíveis para LED e buzzer.
 */
typedef enum
{
    /**
     * Operação normal.
     *
     * LED:
     *      desligado
     *
     * Buzzer:
     *      desligado
     */
    ALARM_MODE_OFF = 0,


    /**
     * Porta aberta, porém ainda dentro
     * do tempo máximo permitido.
     *
     * LED:
     *      continuamente ligado
     *
     * Buzzer:
     *      desligado
     */
    ALARM_MODE_DOOR_OPEN,


    /**
     * Condição crítica.
     *
     * Pode ser provocada por:
     *
     * - porta aberta além do timeout;
     * - temperatura acima do limite;
     * - falha considerada crítica pelo controlador.
     *
     * LED:
     *      piscando
     *
     * Buzzer:
     *      ligado
     */
    ALARM_MODE_CRITICAL

} alarm_mode_t;


/* =========================================================
 * FUNÇÕES PÚBLICAS
 * ========================================================= */

/**
 * @brief Inicializa o sistema de alarme.
 *
 * Configura:
 *
 * - GPIO do LED;
 * - GPIO do buzzer.
 *
 * As duas saídas são inicializadas desligadas.
 *
 * @return
 *      ESP_OK em caso de sucesso.
 *
 *      Código de erro ESP-IDF em caso de falha.
 */
esp_err_t alarm_init(void);


/**
 * @brief Define o modo atual do sistema de alarme.
 *
 * @param mode Novo modo.
 *
 * Exemplos:
 *
 * alarm_set_mode(ALARM_MODE_OFF);
 *
 * alarm_set_mode(ALARM_MODE_DOOR_OPEN);
 *
 * alarm_set_mode(ALARM_MODE_CRITICAL);
 */
void alarm_set_mode(
    alarm_mode_t mode
);


/**
 * @brief Atualiza comportamentos temporizados
 *        do sistema de alarme.
 *
 * Deve ser chamada periodicamente pela tarefa
 * principal.
 *
 * Atualmente é responsável principalmente
 * pelo pisca não bloqueante do LED durante
 * uma condição crítica.
 */
void alarm_update(void);


/**
 * @brief Retorna o modo atual do alarme.
 *
 * @return Estado atual do sistema de alarme.
 */
alarm_mode_t alarm_get_mode(void);