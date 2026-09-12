
#include "mqtt_component.h"

#include <string.h>

#include "esp_log.h"
#include "esp_system.h"

static const char *TAG = "mqtt_component";

static esp_mqtt_client_handle_t mqtt_client = NULL;
static mqtt_message_callback_t message_callback = NULL;
static bool mqtt_connected = false;


/**
 * @brief Handler interno dos eventos MQTT.
 */
static void mqtt_event_handler(
    void *handler_args,
    esp_event_base_t base,
    int32_t event_id,
    void *event_data)
{
    (void)handler_args;
    (void)base;

    esp_mqtt_event_handle_t event = event_data;

    if (event == NULL) {
        return;
    }

    switch ((esp_mqtt_event_id_t)event_id) {

    case MQTT_EVENT_CONNECTED:

        mqtt_connected = true;

        ESP_LOGI(TAG, "MQTT conectado");

        break;


    case MQTT_EVENT_DISCONNECTED:

        mqtt_connected = false;

        ESP_LOGW(TAG, "MQTT desconectado");

        break;


    case MQTT_EVENT_SUBSCRIBED:

        ESP_LOGI(
            TAG,
            "Inscrição realizada, msg_id=%d",
            event->msg_id
        );

        break;


    case MQTT_EVENT_UNSUBSCRIBED:

        ESP_LOGI(
            TAG,
            "Inscrição removida, msg_id=%d",
            event->msg_id
        );

        break;


    case MQTT_EVENT_PUBLISHED:

        ESP_LOGD(
            TAG,
            "Mensagem publicada, msg_id=%d",
            event->msg_id
        );

        break;


    case MQTT_EVENT_DATA:

        ESP_LOGD(TAG, "Mensagem MQTT recebida");

        if (message_callback != NULL) {

            message_callback(
                event->topic,
                event->topic_len,
                event->data,
                event->data_len
            );
        }

        break;


    case MQTT_EVENT_ERROR:

        ESP_LOGE(TAG, "Erro MQTT");

        if (event->error_handle != NULL) {

            ESP_LOGE(
                TAG,
                "Tipo de erro: %d",
                event->error_handle->error_type
            );
        }

        break;


    default:

        ESP_LOGD(
            TAG,
            "Evento MQTT: %d",
            event->event_id
        );

        break;
    }
}


esp_err_t mqtt_init(const mqtt_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (config->broker_uri == NULL) {
        ESP_LOGE(TAG, "Broker URI não configurada");
        return ESP_ERR_INVALID_ARG;
    }

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = config->broker_uri,

        .session.protocol_ver = MQTT_PROTOCOL_V_5,

        .network.disable_auto_reconnect =
            !config->auto_reconnect,
    };


    /*
     * Credenciais
     */
    if (config->username != NULL) {
        mqtt_cfg.credentials.username =
            config->username;
    }

    if (config->password != NULL) {
        mqtt_cfg.credentials.authentication.password =
            config->password;
    }


    /*
     * Last Will
     */
    if (config->will_topic != NULL) {

        mqtt_cfg.session.last_will.topic =
            config->will_topic;

        mqtt_cfg.session.last_will.msg =
            config->will_message;

        if (config->will_message != NULL) {

            mqtt_cfg.session.last_will.msg_len =
                strlen(config->will_message);
        }

        mqtt_cfg.session.last_will.qos =
            config->will_qos;

        mqtt_cfg.session.last_will.retain =
            config->will_retain;
    }


    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);

    if (mqtt_client == NULL) {

        ESP_LOGE(
            TAG,
            "Falha ao inicializar cliente MQTT"
        );

        return ESP_FAIL;
    }


    /*
     * Registro do handler interno.
     */
    esp_err_t err = esp_mqtt_client_register_event(
        mqtt_client,
        ESP_EVENT_ANY_ID,
        mqtt_event_handler,
        NULL
    );

    if (err != ESP_OK) {

        ESP_LOGE(
            TAG,
            "Falha ao registrar event handler"
        );

        esp_mqtt_client_destroy(mqtt_client);
        mqtt_client = NULL;

        return err;
    }


    ESP_LOGI(TAG, "Componente MQTT inicializado");

    return ESP_OK;
}


esp_err_t mqtt_start(void)
{
    if (mqtt_client == NULL) {
        ESP_LOGE(TAG, "MQTT não foi inicializado");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = esp_mqtt_client_start(mqtt_client);

    if (err != ESP_OK) {

        ESP_LOGE(
            TAG,
            "Falha ao iniciar MQTT: %s",
            esp_err_to_name(err)
        );
    }

    return err;
}


esp_err_t mqtt_stop(void)
{
    if (mqtt_client == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    mqtt_connected = false;

    return esp_mqtt_client_stop(mqtt_client);
}


int mqtt_publish(
    const char *topic,
    const char *data,
    int qos,
    int retain)
{
    if (mqtt_client == NULL) {
        ESP_LOGE(TAG, "MQTT não inicializado");
        return -1;
    }

    if (topic == NULL || data == NULL) {
        return -1;
    }

    return esp_mqtt_client_publish(
        mqtt_client,
        topic,
        data,
        0,
        qos,
        retain
    );
}


int mqtt_subscribe(
    const char *topic,
    int qos)
{
    if (mqtt_client == NULL) {
        ESP_LOGE(TAG, "MQTT não inicializado");
        return -1;
    }

    if (topic == NULL) {
        return -1;
    }

    return esp_mqtt_client_subscribe(
        mqtt_client,
        topic,
        qos
    );
}


int mqtt_unsubscribe(
    const char *topic)
{
    if (mqtt_client == NULL) {
        ESP_LOGE(TAG, "MQTT não inicializado");
        return -1;
    }

    if (topic == NULL) {
        return -1;
    }

    return esp_mqtt_client_unsubscribe(
        mqtt_client,
        topic
    );
}


void mqtt_set_message_callback(
    mqtt_message_callback_t callback)
{
    message_callback = callback;
}


bool mqtt_is_connected(void)
{
    return mqtt_connected;
}


esp_mqtt_client_handle_t mqtt_get_client(void)
{
    return mqtt_client;
}

