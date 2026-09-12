#ifndef APP_CONTROLLER_H
#define APP_CONTROLLER_H

#include "esp_err.h"

/*
 * Inicializa todos os módulos utilizados
 * pelo controlador.
 */
esp_err_t app_controller_init(void);

/*
 * Cria e inicia a tarefa principal
 * do controlador.
 */
esp_err_t app_controller_start(void);

#endif