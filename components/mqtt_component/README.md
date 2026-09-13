# Componente MQTT

Componente de apoio para inicializar o cliente MQTT da ESP-IDF, publicar mensagens, assinar tópicos, receber mensagens por callback e consultar o estado da conexão.

## Inicialização

Inclua o cabeçalho do componente:

```c
#include "mqtt_component.h"
```

Configure o broker:

```c
mqtt_config_t config = {
    .broker_uri = "mqtt://192.168.1.100:1883",
    .username = "usuario",
    .password = "senha",
    .auto_reconnect = true
};

mqtt_init(&config);
mqtt_start();
```

## Publicar

```c
mqtt_publish(
    "sensor/temperatura",
    "-18.5",
    1,
    0
);
```

| Parâmetro | Descrição |
| --- | --- |
| `topic` | Tópico MQTT |
| `data` | Payload |
| `qos` | QoS 0, 1 ou 2 |
| `retain` | 0 = não retém, 1 = retém |

## Assinar

```c
mqtt_subscribe(
    "sensor/comando",
    1
);
```

## Cancelar inscrição

```c
mqtt_unsubscribe(
    "sensor/comando"
);
```

## Receber mensagens

Crie um callback:

```c
void minha_callback(
    const char *topic,
    int topic_len,
    const char *data,
    int data_len)
{
    printf(
        "%.*s -> %.*s\n",
        topic_len,
        topic,
        data_len,
        data
    );
}
```

Registre:

```c
mqtt_set_message_callback(minha_callback);
```

## Verificar conexão

```c
if (mqtt_is_connected()) {
    printf("MQTT conectado\n");
}
```

## Parar MQTT

```c
mqtt_stop();
```

## Last Will

O componente suporta Last Will:

```c
mqtt_config_t config = {
    .broker_uri = "mqtt://192.168.1.100:1883",
    .will_topic = "device/status",
    .will_message = "offline",
    .will_qos = 1,
    .will_retain = true,
    .auto_reconnect = true
};
```

Nesse exemplo, o dispositivo pode publicar `offline` automaticamente no tópico `device/status` quando a conexão for encerrada de maneira inesperada.

## Responsabilidade da aplicação

O componente MQTT não inicializa Wi-Fi ou Ethernet.

A aplicação deve estabelecer a conexão de rede antes de chamar:

```c
mqtt_init(&config);
mqtt_start();
```
