#pragma once

#include <stdbool.h>
#include <stdint.h>


/* =========================================================
 * ESTADO DO ALGORITMO ADAPTATIVO
 * ========================================================= */

/**
 * @brief Armazena as informações necessárias para aprender
 *        o comportamento térmico da câmara e calcular o
 *        timeout adaptativo.
 */
typedef struct
{
    /* =====================================================
     * BASELINE TÉRMICO
     * ===================================================== */

    /**
     * Indica se já possuímos uma temperatura média
     * de referência válida.
     */
    bool baseline_valid;


    /**
     * Temperatura média de referência da câmara.
     *
     * Essa temperatura é aprendida lentamente enquanto:
     *
     * - a porta está fechada;
     * - não existe alarme;
     * - a temperatura está na faixa normal.
     *
     * Unidade: °C.
     */
    float baseline_temp_c;


    /* =====================================================
     * HISTÓRICO DE RECUPERAÇÃO
     * ===================================================== */

    /**
     * Indica se já existe pelo menos uma medição
     * válida do tempo de recuperação térmica.
     */
    bool recovery_valid;


    /**
     * Média móvel exponencial dos tempos
     * de recuperação observados.
     *
     * Unidade: segundos.
     */
    float recovery_ema_s;


    /* =====================================================
     * RECUPERAÇÃO ATUAL
     * ===================================================== */

    /**
     * Indica se estamos acompanhando uma
     * recuperação térmica neste momento.
     */
    bool recovery_active;


    /**
     * Temperatura que existia antes da abertura
     * da porta e para a qual queremos retornar.
     *
     * Unidade: °C.
     */
    float recovery_reference_c;


    /**
     * Momento em que a recuperação começou.
     *
     * O valor é obtido através de:
     *
     * esp_timer_get_time()
     *
     * Unidade: microssegundos.
     */
    int64_t recovery_started_us;


    /**
     * Momento em que a temperatura entrou
     * na região considerada recuperada.
     */
    int64_t recovery_stable_started_us;


    /**
     * Indica se atualmente estamos dentro
     * da região de estabilidade:
     *
     * baseline ± tolerância.
     */
    bool recovery_in_stable_zone;

} adaptive_timeout_t;


/* =========================================================
 * FUNÇÕES PÚBLICAS
 * ========================================================= */

/**
 * @brief Inicializa a estrutura do algoritmo adaptativo.
 *
 * @param adaptive Estrutura que será inicializada.
 */
void adaptive_timeout_init(
    adaptive_timeout_t *adaptive
);


/**
 * @brief Atualiza a temperatura média de referência
 *        da câmara.
 *
 * O baseline somente é atualizado em condições
 * consideradas adequadas para aprendizado.
 *
 * @param adaptive Estrutura do algoritmo.
 *
 * @param temperature_c Temperatura atual.
 *
 * @param door_closed true quando a porta estiver fechada.
 *
 * @param temperature_alarm true quando existir
 *                          alarme térmico.
 */
void adaptive_timeout_update_baseline(
    adaptive_timeout_t *adaptive,
    float temperature_c,
    bool door_closed,
    bool temperature_alarm
);


/**
 * @brief Inicia o acompanhamento da recuperação térmica
 *        depois que a porta é fechada.
 *
 * @param adaptive Estrutura do algoritmo.
 *
 * @param reference_temperature_c Temperatura de referência
 *                                anterior à abertura.
 *
 * @param now_us Tempo atual em microssegundos.
 */
void adaptive_timeout_start_recovery(
    adaptive_timeout_t *adaptive,
    float reference_temperature_c,
    int64_t now_us
);


/**
 * @brief Atualiza o processo de recuperação térmica.
 *
 * Verifica se a temperatura retornou para próximo
 * da referência e se permaneceu estável durante
 * o período mínimo configurado.
 *
 * Quando a recuperação termina, o tempo observado
 * é incorporado ao histórico através de uma
 * média móvel exponencial.
 *
 * @param adaptive Estrutura do algoritmo.
 *
 * @param temperature_c Temperatura atual.
 *
 * @param now_us Tempo atual em microssegundos.
 */
void adaptive_timeout_update_recovery(
    adaptive_timeout_t *adaptive,
    float temperature_c,
    int64_t now_us
);


/**
 * @brief Calcula o tempo máximo permitido para
 *        uma nova abertura da porta.
 *
 * O cálculo considera:
 *
 * 1. timeout base;
 * 2. temperatura atual;
 * 3. histórico de recuperação térmica.
 *
 * Implementação:
 *
 * timeout =
 *
 *      timeout_base
 *      × fator_temperatura
 *      × fator_recuperacao
 *
 * O resultado final é limitado pelos valores:
 *
 * OLAF_TIMEOUT_MIN_S
 *
 * e
 *
 * OLAF_TIMEOUT_MAX_S.
 *
 * @param adaptive Estado atual do algoritmo.
 *
 * @param current_temperature_c Temperatura existente
 *                              no momento da abertura.
 *
 * @return Timeout em segundos.
 */
float adaptive_timeout_calculate(
    const adaptive_timeout_t *adaptive,
    float current_temperature_c
);