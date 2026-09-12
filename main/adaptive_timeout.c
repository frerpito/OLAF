#include "adaptive_timeout.h"
#include "app_config.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#include "esp_log.h"


static const char *TAG = "ADAPTIVE_TIMEOUT";


/* =========================================================
 * FUNÇÕES AUXILIARES
 * ========================================================= */

/**
 * @brief Limita um valor entre mínimo e máximo.
 */
static float clamp_float(
    float value,
    float minimum,
    float maximum)
{
    if (value < minimum)
    {
        return minimum;
    }

    if (value > maximum)
    {
        return maximum;
    }

    return value;
}


/**
 * @brief Calcula uma média móvel exponencial.
 *
 * Fórmula:
 *
 * EMA_n =
 *      alpha * valor_n
 *      +
 *      (1 - alpha) * EMA_anterior
 */
static float ema_update(
    float previous,
    float new_value,
    float alpha)
{
    return
        (alpha * new_value)
        +
        ((1.0f - alpha) * previous);
}




void adaptive_timeout_init(
    adaptive_timeout_t *adaptive)
{
    if (adaptive == NULL)
    {
        return;
    }




    adaptive->baseline_valid = false;

    adaptive->baseline_temp_c = 0.0f;

    adaptive->recovery_valid = false;

    adaptive->recovery_ema_s = 0.0f;

    adaptive->recovery_active = false;

    adaptive->recovery_reference_c = 0.0f;

    adaptive->recovery_started_us = 0;

    adaptive->recovery_stable_started_us = 0;

    adaptive->recovery_in_stable_zone = false;


    ESP_LOGI(
        TAG,
        "Algoritmo adaptativo inicializado"
    );
}


/* =========================================================
 * TEMPERATURA MÉDIA / BASELINE
 * ========================================================= */

void adaptive_timeout_update_baseline(
    adaptive_timeout_t *adaptive,
    float temperature_c,
    bool door_closed,
    bool temperature_alarm)
{
    if (adaptive == NULL)
    {
        return;
    }


    if (!isfinite(temperature_c))
    {
        return;
    }


    /*
     * A temperatura média da câmara somente
     * deve ser aprendida quando a porta
     * estiver fechada.
     *
     * Isso evita que o aquecimento provocado
     * pela abertura passe a ser considerado
     * "normal".
     */

    if (!door_closed)
    {
        return;
    }


    /*
     * Também não aprendemos durante uma
     * condição de alarme.
     */

    if (temperature_alarm)
    {
        return;
    }


    /*
     * Só utilizamos temperaturas dentro da
     * faixa considerada normal.
     */

    if (
        temperature_c < OLAF_TEMP_NORMAL_MIN_C
        ||
        temperature_c > OLAF_TEMP_NORMAL_MAX_C
    )
    {
        return;
    }


    /* =====================================================
     * PRIMEIRA LEITURA
     * ===================================================== */

    if (!adaptive->baseline_valid)
    {
        adaptive->baseline_temp_c =
            temperature_c;

        adaptive->baseline_valid =
            true;


        ESP_LOGI(
            TAG,
            "Baseline inicial: %.2f C",
            adaptive->baseline_temp_c
        );


        return;
    }


    /* =====================================================
     * MÉDIA MÓVEL EXPONENCIAL
     * =====================================================
     *
     * Em vez de:
     *
     * media = soma / quantidade
     *
     * utilizamos EMA.
     *
     * Isso permite acompanhar lentamente
     * mudanças naturais da temperatura
     * da câmara sem armazenar centenas
     * de amostras.
     */

    adaptive->baseline_temp_c =
        ema_update(
            adaptive->baseline_temp_c,
            temperature_c,
            OLAF_BASELINE_ALPHA
        );
}


/* =========================================================
 * INÍCIO DA RECUPERAÇÃO TÉRMICA
 * ========================================================= */

void adaptive_timeout_start_recovery(
    adaptive_timeout_t *adaptive,
    float reference_temperature_c,
    int64_t now_us)
{
    if (adaptive == NULL)
    {
        return;
    }


    /*
     * Se não temos uma temperatura de referência
     * válida, não podemos medir a recuperação. Né frerp
     */

    if (!isfinite(reference_temperature_c))
    {
        return;
    }


    adaptive->recovery_active =
        true;


    /*
     * Essa é a temperatura que existia
     * aproximadamente antes da abertura.
     */

    adaptive->recovery_reference_c =
        reference_temperature_c;


    /*
     * Momento em que a porta foi fechada
     * e a recuperação começou.
     */

    adaptive->recovery_started_us =
        now_us;


    /*
     * Ainda não entramos na região considerada
     * recuperada.
     */

    adaptive->recovery_in_stable_zone =
        false;


    adaptive->recovery_stable_started_us =
        0;


    ESP_LOGI(
        TAG,
        "Monitoramento da recuperacao iniciado"
    );


    ESP_LOGI(
        TAG,
        "Temperatura de referencia: %.2f C",
        reference_temperature_c
    );
}


/* =========================================================
 * ATUALIZAÇÃO DA RECUPERAÇÃO
 * ========================================================= */

void adaptive_timeout_update_recovery(
    adaptive_timeout_t *adaptive,
    float temperature_c,
    int64_t now_us)
{
    if (adaptive == NULL)
    {
        return;
    }


    /*
     * Se nenhuma recuperação estiver ativa,
     * não há nada para fazer.
     */

    if (!adaptive->recovery_active)
    {
        return;
    }


    if (!isfinite(temperature_c))
    {
        return;
    }


    /* =====================================================
     * TOLERÂNCIA DE RECUPERAÇÃO
     * =====================================================
     *
     * Consideramos que a temperatura voltou
     * para próximo do valor anterior quando
     * estiver dentro de ±0.5 °C.
     *
     * Exemplo:
     *
     * referência = 2.5 °C
     *
     * faixa:
     *
     * 2.0 °C até 3.0 °C
     */

    const float recovery_tolerance_c =
        0.5f;


    const float difference =
        fabsf(
            temperature_c
            -
            adaptive->recovery_reference_c
        );


    /* =====================================================
     * ENTROU NA REGIÃO DE RECUPERAÇÃO
     * ===================================================== */

    if (difference <= recovery_tolerance_c)
    {
        /*
         * Primeira leitura dentro da faixa.
         */

        if (!adaptive->recovery_in_stable_zone)
        {
            adaptive->recovery_in_stable_zone =
                true;


            adaptive->recovery_stable_started_us =
                now_us;


            ESP_LOGI(
                TAG,
                "Temperatura retornou proxima ao baseline"
            );
        }


        /* =================================================
         * VERIFICA ESTABILIDADE
         * ================================================= */

        const float stable_time_s =
            (float)(
                now_us
                -
                adaptive->recovery_stable_started_us
            )
            /
            1000000.0f;


        /*
         * Não basta entrar momentaneamente
         * na faixa.
         *
         * A temperatura precisa permanecer
         * estável durante o período definido.
         */

        if (
            stable_time_s >=
            OLAF_RECOVERY_STABLE_S
        )
        {
            /* =============================================
             * TEMPO TOTAL DE RECUPERAÇÃO
             * ============================================= */

            const float recovery_time_s =
                (float)(
                    now_us
                    -
                    adaptive->recovery_started_us
                )
                /
                1000000.0f;


            /*
             * Primeira recuperação observada.
             */

            if (!adaptive->recovery_valid)
            {
                adaptive->recovery_ema_s =
                    recovery_time_s;


                adaptive->recovery_valid =
                    true;
            }

            /*
             * Já temos histórico.
             *
             * Atualizamos utilizando EMA.
             */

            else
            {
                adaptive->recovery_ema_s =
                    ema_update(
                        adaptive->recovery_ema_s,
                        recovery_time_s,
                        OLAF_RECOVERY_ALPHA
                    );
            }


            ESP_LOGI(
                TAG,
                "Recuperacao concluida"
            );


            ESP_LOGI(
                TAG,
                "Tempo desta recuperacao: %.1f s",
                recovery_time_s
            );


            ESP_LOGI(
                TAG,
                "Media historica de recuperacao: %.1f s",
                adaptive->recovery_ema_s
            );


            /*
             * Finaliza o acompanhamento
             * deste evento.
             */

            adaptive->recovery_active =
                false;


            adaptive->recovery_in_stable_zone =
                false;
        }
    }


    /* =====================================================
     * SAIU DA REGIÃO DE RECUPERAÇÃO
     * ===================================================== */

    else
    {
        /*
         * Se entrou na região de ±0.5 °C,
         * mas saiu antes dos 20 segundos,
         * reiniciamos a contagem de estabilidade.
         */

        adaptive->recovery_in_stable_zone =
            false;


        adaptive->recovery_stable_started_us =
            0;
    }
}


/* =========================================================
 * FATOR DE TEMPERATURA
 * ========================================================= */

static float calculate_temperature_factor(
    float temperature_c)
{
    /*
     * Temperaturas iguais ou inferiores ao
     * limite inferior normal recebem fator 1.
     */

    if (
        temperature_c <=
        OLAF_TEMP_NORMAL_MIN_C
    )
    {
        return 1.0f;
    }


    /*
     * Se já estamos na temperatura de alarme,
     * reduzimos bastante o tempo permitido.
     */

    if (
        temperature_c >=
        OLAF_TEMP_ALARM_HIGH_C
    )
    {
        return 0.25f;
    }


    /*
     *
     * Quanto maior a temperatura,
     * menor o tempo permitido.
     */

    const float range =
        OLAF_TEMP_ALARM_HIGH_C
        -
        OLAF_TEMP_NORMAL_MIN_C;


    const float position =
        (
            temperature_c
            -
            OLAF_TEMP_NORMAL_MIN_C
        )
        /
        range;


    const float factor =
        1.0f
        -
        (
            position
            *
            0.75f
        );


    return clamp_float(
        factor,
        0.25f,
        1.0f
    );
}


/* =========================================================
 * FATOR DE RECUPERAÇÃO
 * ========================================================= */

static float calculate_recovery_factor(
    const adaptive_timeout_t *adaptive)
{
    /*
     * Se ainda não aprendemos o comportamento
     * da câmara, utilizamos fator neutro.
     */

    if (
        adaptive == NULL
        ||
        !adaptive->recovery_valid
    )
    {
        return 1.0f;
    }


    if (adaptive->recovery_ema_s <= 0.0f)
    {
        return 1.0f;
    }


    float factor =
        OLAF_RECOVERY_TARGET_S
        /
        adaptive->recovery_ema_s;


    /*
     * Não permitimos que o histórico:
     *
     * - reduza para menos de 50%;
     * - aumente para mais de 115%.
     */

    factor =
        clamp_float(
            factor,
            0.50f,
            1.15f
        );


    return factor;
}


/* =========================================================
 * CÁLCULO DO TIMEOUT ADAPTATIVO
 * ========================================================= */

float adaptive_timeout_calculate(
    const adaptive_timeout_t *adaptive,
    float current_temperature_c)
{
    /*
     * Se a temperatura estiver inválida,
     * usamos o timeout mínimo como
     * estratégia conservadora.
     */

    if (!isfinite(current_temperature_c))
    {
        return OLAF_TIMEOUT_MIN_S;
    }


    /* =====================================================
     * FATOR TÉRMICO
     * ===================================================== */

    const float temperature_factor =
        calculate_temperature_factor(
            current_temperature_c
        );


    /* =====================================================
     * FATOR HISTÓRICO
     * ===================================================== */

    const float recovery_factor =
        calculate_recovery_factor(
            adaptive
        );


    /* =====================================================
     * TIMEOUT
     * =====================================================
     *
     * Fórmula utilizada:
     *
     * timeout =
     *
     * timeout_base
     *
     * ×
     *
     * fator_temperatura
     *
     * ×
     *
     * fator_recuperacao
     */

    float timeout_s =
        OLAF_TIMEOUT_BASE_S
        *
        temperature_factor
        *
        recovery_factor;


    /*
     * Mantemos o resultado dentro dos
     * limites de segurança configurados.
     */

    timeout_s =
        clamp_float(
            timeout_s,
            OLAF_TIMEOUT_MIN_S,
            OLAF_TIMEOUT_MAX_S
        );


    ESP_LOGI(
        TAG,
        "Timeout calculado: %.1f s | "
        "fatorTemp=%.3f | "
        "fatorRecovery=%.3f",
        timeout_s,
        temperature_factor,
        recovery_factor
    );


    return timeout_s;
}