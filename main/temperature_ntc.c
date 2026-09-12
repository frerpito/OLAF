#include "temperature_ntc.h"
#include "app_config.h"

#include <math.h>
#include <stdint.h>

#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

#include "esp_err.h"
#include "esp_log.h"


static const char *TAG = "NTC";


/* =========================================================
 * HANDLES DO ADC
 * ========================================================= */

/*
 * Handle da unidade ADC utilizada para leitura do NTC.
 */
static adc_oneshot_unit_handle_t s_adc_handle = NULL;


/*
 * Handle utilizado pelo sistema de calibração
 * do ADC.
 */
static adc_cali_handle_t s_adc_cali_handle = NULL;


/*
 * Indica se conseguimos inicializar corretamente
 * a calibração do ADC.
 */
static bool s_adc_calibrated = false;


/* =========================================================
 * CONVERSÃO DE TENSÃO PARA RESISTÊNCIA
 * ========================================================= */

/**
 * @brief Calcula a resistência do NTC a partir
 *        da tensão medida no divisor resistivo.
 *
 * Circuito considerado:
 *
 *          3.3 V
 *            |
 *        R_FIXED
 *          10k
 *            |
 *            +---------- ADC
 *            |
 *           NTC
 *            |
 *           GND
 *
 *
 * Para esse circuito:
 *
 *              Rntc
 * Vout = Vcc ---------
 *            Rfix+Rntc
 *
 *
 * Isolando Rntc:
 *
 *             Rfix * Vout
 * Rntc = ---------------------
 *              Vcc - Vout
 */
static float ntc_resistance_from_voltage(float voltage_mv)
{
    /*
     * Proteção contra divisão por zero
     * e valores fisicamente inválidos.
     */
    if (voltage_mv <= 1.0f ||
        voltage_mv >= (OLAF_ADC_SUPPLY_MV - 1.0f))
    {
        return NAN;
    }


    const float resistance =
        OLAF_NTC_FIXED_R_OHM
        *
        voltage_mv
        /
        (OLAF_ADC_SUPPLY_MV - voltage_mv);


    return resistance;
}


/* =========================================================
 * CONVERSÃO DE RESISTÊNCIA PARA TEMPERATURA
 * ========================================================= */

/**
 * @brief Converte a resistência do NTC para °C
 *        utilizando a equação Beta.
 */
static float ntc_temperature_from_resistance(
    float resistance_ohm)
{
    /*
     * Verifica se recebemos uma resistência válida.
     */
    if (!isfinite(resistance_ohm) ||
        resistance_ohm <= 0.0f)
    {
        return NAN;
    }


    /*
     * A equação utiliza temperatura absoluta.
     *
     * Portanto:
     *
     * Kelvin = Celsius + 273.15
     */

    const float t0_kelvin =
        OLAF_NTC_T0_C + 273.15f;


    /*
     * Equação Beta:
     *
     * 1/T =
     *      1/T0
     *      +
     *      (1/Beta) * ln(R/R0)
     */

    const float inverse_temperature =
        (1.0f / t0_kelvin)
        +
        (1.0f / OLAF_NTC_BETA)
        *
        logf(
            resistance_ohm /
            OLAF_NTC_R0_OHM
        );


    /*
     * Recuperamos T em Kelvin.
     */

    const float temperature_kelvin =
        1.0f /
        inverse_temperature;


    /*
     * Converte Kelvin para Celsius.
     */

    const float temperature_c =
        temperature_kelvin -
        273.15f;


    return temperature_c;
}


/* =========================================================
 * CALIBRAÇÃO DO ADC
 * ========================================================= */

static void initialize_adc_calibration(void)
{
    /*
     * ESP32-S3 suporta calibração do ADC.
     *
     * Tentamos utilizar Curve Fitting.
     */

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED

    adc_cali_curve_fitting_config_t cali_config =
    {
        .unit_id = OLAF_NTC_ADC_UNIT,

        .chan = OLAF_NTC_ADC_CHANNEL,

        .atten = OLAF_NTC_ADC_ATTEN,

        .bitwidth = ADC_BITWIDTH_DEFAULT
    };


    esp_err_t err =
        adc_cali_create_scheme_curve_fitting(
            &cali_config,
            &s_adc_cali_handle
        );


    if (err == ESP_OK)
    {
        s_adc_calibrated = true;


        ESP_LOGI(
            TAG,
            "Calibracao ADC Curve Fitting habilitada"
        );


        return;
    }

#endif


    /*
     * Caso a calibração não esteja disponível,
     * continuamos funcionando.
     *
     * Entretanto, a precisão será inferior.
     */

    s_adc_calibrated = false;


    ESP_LOGW(
        TAG,
        "Calibracao ADC indisponivel"
    );
}


/* =========================================================
 * INICIALIZAÇÃO DO NTC
 * ========================================================= */

esp_err_t temperature_ntc_init(void)
{
    /* =====================================================
     * 1. CRIA UNIDADE ADC
     * ===================================================== */

    adc_oneshot_unit_init_cfg_t unit_config =
    {
        .unit_id = OLAF_NTC_ADC_UNIT,

        .ulp_mode = ADC_ULP_MODE_DISABLE
    };


    esp_err_t err =
        adc_oneshot_new_unit(
            &unit_config,
            &s_adc_handle
        );


    if (err != ESP_OK)
    {
        ESP_LOGE(
            TAG,
            "Erro ao inicializar ADC: %s",
            esp_err_to_name(err)
        );


        return err;
    }


    /* =====================================================
     * 2. CONFIGURA CANAL
     * ===================================================== */

    adc_oneshot_chan_cfg_t channel_config =
    {
        .atten = OLAF_NTC_ADC_ATTEN,

        .bitwidth = ADC_BITWIDTH_DEFAULT
    };


    err =
        adc_oneshot_config_channel(
            s_adc_handle,
            OLAF_NTC_ADC_CHANNEL,
            &channel_config
        );


    if (err != ESP_OK)
    {
        ESP_LOGE(
            TAG,
            "Erro ao configurar canal ADC: %s",
            esp_err_to_name(err)
        );


        return err;
    }


    /* =====================================================
     * 3. INICIALIZA CALIBRAÇÃO
     * ===================================================== */

    initialize_adc_calibration();


    ESP_LOGI(
        TAG,
        "Sensor NTC inicializado"
    );


    ESP_LOGI(
        TAG,
        "Numero de amostras por leitura: %d",
        OLAF_TEMP_ADC_SAMPLES
    );


    return ESP_OK;
}


/* =========================================================
 * LEITURA DA TEMPERATURA
 * ========================================================= */

esp_err_t temperature_ntc_read(
    temperature_reading_t *out)
{
    /*
     * Verifica se recebemos um ponteiro válido.
     */

    if (out == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }


    /*
     * Verifica se o ADC foi inicializado.
     */

    if (s_adc_handle == NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }


    /* =====================================================
     * ACUMULADORES
     * ===================================================== */

    int64_t raw_sum = 0;

    int64_t voltage_sum_mv = 0;


    /* =====================================================
     * MULTISAMPLING
     * =====================================================
     *
     * Realizamos várias leituras do ADC.
     *
     * Exemplo:
     *
     * 1834
     * 1839
     * 1836
     * 1842
     * ...
     *
     * Depois calculamos a média.
     */

    for (
        int sample = 0;
        sample < OLAF_TEMP_ADC_SAMPLES;
        sample++
    )
    {
        int raw = 0;


        /*
         * Faz uma conversão ADC.
         */

        esp_err_t err =
            adc_oneshot_read(
                s_adc_handle,
                OLAF_NTC_ADC_CHANNEL,
                &raw
            );


        if (err != ESP_OK)
        {
            ESP_LOGW(
                TAG,
                "Falha na leitura ADC: %s",
                esp_err_to_name(err)
            );


            return err;
        }


        /*
         * Soma o valor bruto.
         */

        raw_sum += raw;


        /*
         * Se a calibração estiver disponível,
         * convertemos cada leitura para mV.
         */

        if (s_adc_calibrated)
        {
            int voltage_mv = 0;


            err =
                adc_cali_raw_to_voltage(
                    s_adc_cali_handle,
                    raw,
                    &voltage_mv
                );


            if (err == ESP_OK)
            {
                voltage_sum_mv +=
                    voltage_mv;
            }
            else
            {
                ESP_LOGW(
                    TAG,
                    "Falha na conversao ADC calibrada"
                );


                /*
                 * Desabilita calibração para esta
                 * e futuras leituras.
                 */
                s_adc_calibrated = false;
            }
        }
    }


    /* =====================================================
     * MÉDIA DAS LEITURAS ADC
     * ===================================================== */

    const float raw_average =
        (float)raw_sum /
        (float)OLAF_TEMP_ADC_SAMPLES;


    /*
     * Agora precisamos transformar o resultado
     * em tensão.
     */

    float voltage_mv = 0.0f;


    /* =====================================================
     * ADC CALIBRADO
     * ===================================================== */

    if (s_adc_calibrated)
    {
        voltage_mv =
            (float)voltage_sum_mv /
            (float)OLAF_TEMP_ADC_SAMPLES;
    }


    /* =====================================================
     * FALLBACK SEM CALIBRAÇÃO
     * ===================================================== */

    else
    {
        /*
         * Aproximação simples.
         *
         * ADC 12 bits:
         *
         * 0 ... 4095
         *
         * Essa conversão é apenas um fallback.
         * Para a medição final do projeto devemos
         * preferir a calibração do ADC.
         */

        const float adc_max =
            4095.0f;


        voltage_mv =
            (
                raw_average /
                adc_max
            )
            *
            OLAF_ADC_SUPPLY_MV;
    }


    /* =====================================================
     * TENSÃO -> RESISTÊNCIA
     * ===================================================== */

    const float resistance_ohm =
        ntc_resistance_from_voltage(
            voltage_mv
        );


    /* =====================================================
     * RESISTÊNCIA -> TEMPERATURA
     * ===================================================== */

    const float temperature_c =
        ntc_temperature_from_resistance(
            resistance_ohm
        );


    /* =====================================================
     * PREENCHE RESULTADO
     * ===================================================== */

    out->voltage_mv =
        voltage_mv;


    out->resistance_ohm =
        resistance_ohm;


    out->temperature_c =
        temperature_c;


    /*
     * Faz uma validação simples.
     *
     * Como estamos trabalhando com uma
     * câmara frigorífica, valores muito fora
     * desta faixa provavelmente indicam:
     *
     * - NTC desconectado;
     * - curto;
     * - problema no divisor;
     * - erro de ADC.
     */

    out->valid =
        isfinite(temperature_c)
        &&
        temperature_c > -60.0f
        &&
        temperature_c < 80.0f;


    /* =====================================================
     * VERIFICA RESULTADO
     * ===================================================== */

    if (!out->valid)
    {
        ESP_LOGW(
            TAG,
            "Temperatura invalida | "
            "ADC=%.1f | "
            "V=%.1f mV | "
            "R=%.1f ohm",
            raw_average,
            voltage_mv,
            resistance_ohm
        );


        return ESP_ERR_INVALID_RESPONSE;
    }


    return ESP_OK;
}