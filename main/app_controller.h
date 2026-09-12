#pragma once

#include "esp_err.h"


/**
 * @brief Inicializa o controlador principal do sistema OLAF.
 *
 * Inicializa os módulos responsáveis por:
 *
 * - sensor da porta;
 * - sensor de temperatura;
 * - LED;
 * - buzzer;
 * - algoritmo de timeout adaptativo.
 *
 * @return
 *      - ESP_OK em caso de sucesso;
 *      - código de erro ESP-IDF em caso de falha.
 */
esp_err_t app_controller_init(void);


/**
 * @brief Tarefa principal de controle do sistema.
 *
 * Essa tarefa executa continuamente:
 *
 * - leitura do estado da porta;
 * - detecção de abertura e fechamento;
 * - leitura periódica da temperatura;
 * - controle do tempo de abertura;
 * - verificação do timeout;
 * - acompanhamento da recuperação térmica;
 * - atualização do baseline térmico;
 * - controle do LED;
 * - controle do buzzer.
 *
 * @param arg Parâmetro da tarefa.
 *            Atualmente não utilizado.
 */
void app_controller_task(void *arg);