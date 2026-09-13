
#ifndef MQTT_COMPONENT_H
#define MQTT_COMPONENT_H

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"
#include "mqtt_client.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Configuração do componente MQTT.
 */
typedef struct {
    const char *broker_uri;
    const char *username;
    const char *password;

    /**
     * @brief QoS utilizado pelo Last Will.
     */
    int will_qos;

    /**
     * @brief Tópico do Last Will.
     * NULL para desabilitar.
     */
    const char *will_topic;

    /**
     * @brief Mensagem do Last Will.
     */
    const char *will_message;

    /**
     * @brief Retain do Last Will.
     */
    bool will_retain;

    /**
     * @brief Habilita reconexão automática.
     */
    bool auto_reconnect;
} mqtt_config_t;


/**
 * @brief Callback chamado quando uma mensagem MQTT é recebida.
 *
 * @param topic     Tópico recebido.
 * @param topic_len Tamanho do tópico.
 * @param data      Payload recebido.
 * @param data_len  Tamanho do payload.
 */
typedef void (*mqtt_message_callback_t)(
    const char *topic,
    int topic_len,
    const char *data,
    int data_len
);


/**
 * @brief Inicializa o componente MQTT.
 *
 * @param config Configuração do broker e cliente.
 *
 * @return ESP_OK em caso de sucesso.
 */
esp_err_t mqtt_init(const mqtt_config_t *config);


/**
 * @brief Inicia a conexão MQTT.
 *
 * @return ESP_OK em caso de sucesso.
 */
esp_err_t mqtt_start(void);


/**
 * @brief Para o cliente MQTT.
 *
 * @return ESP_OK em caso de sucesso.
 */
esp_err_t mqtt_stop(void);


/**
 * @brief Publica uma mensagem MQTT.
 *
 * @param topic  Tópico.
 * @param data   Payload.
 * @param qos    QoS (0, 1 ou 2).
 * @param retain Retain da mensagem.
 *
 * @return ID da mensagem ou erro negativo.
 */
int mqtt_publish(
    const char *topic,
    const char *data,
    int qos,
    int retain
);


/**
 * @brief Inscreve o cliente em um tópico.
 *
 * @param topic Tópico.
 * @param qos   QoS da assinatura.
 *
 * @return ID da mensagem ou erro negativo.
 */
int mqtt_subscribe(
    const char *topic,
    int qos
);


/**
 * @brief Remove uma inscrição MQTT.
 *
 * @param topic Tópico.
 *
 * @return ID da mensagem ou erro negativo.
 */
int mqtt_unsubscribe(
    const char *topic
);


/**
 * @brief Registra o callback para mensagens recebidas.
 *
 * @param callback Função chamada quando chegar uma mensagem.
 */
void mqtt_set_message_callback(
    mqtt_message_callback_t callback
);


/**
 * @brief Informa se o cliente está conectado ao broker.
 *
 * @return true se conectado.
 */
bool mqtt_is_connected(void);


/**
 * @brief Retorna o handle interno do cliente MQTT.
 *
 * @return Handle MQTT ou NULL.
 *
 * @note Deve ser usado somente quando for necessário acessar
 * diretamente recursos da ESP-IDF não expostos pelo componente.
 */
esp_mqtt_client_handle_t mqtt_get_client(void);

#ifdef __cplusplus
}
#endif

#endif /* MQTT_COMPONENT_H */

