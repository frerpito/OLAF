# Arquitetura Final Do OLAF

Este documento descreve a arquitetura final do OLAF para facilitar reprodução, manutenção e avaliação do projeto.

## Visão Geral

O sistema usa um ESP32 para ler sensores, processar as regras de alerta localmente e enviar dados por MQTT.

```mermaid
flowchart LR
    Porta[Sensor de porta E18-D80NK] --> Door[Componente door_sensor]
    NTC[NTC 10K + divisor de tensao] --> Temp[Componente temperature_ntc]
    Door --> Controller[app_controller]
    Temp --> Controller
    Controller --> Adaptive[adaptive_timeout]
    Controller --> Alarm[alarm]
    Alarm --> DoorLed[LED vermelho da porta]
    Alarm --> TempLed[LED azul de temperatura]
    Alarm --> RecoveryLed[LED amarelo de recuperacao]
    Alarm --> Buzzer[Buzzer ativo]
    Controller --> MQTT[mqtt_component]
    Main[app_main] --> WiFi[wifi_component]
    WiFi --> MQTT
    MQTT --> Broker[Broker MQTT]
    Broker --> Dashboard[Dashboard/cliente MQTT]
```

## Sequência De Inicialização

```mermaid
sequenceDiagram
    participant Main as app_main
    participant WiFi as wifi_component
    participant MQTT as mqtt_component
    participant App as app_controller
    participant Alarm as alarm

    Main->>WiFi: wifi_connect()
    WiFi-->>Main: ESP_OK apos obter IP
    Main->>MQTT: mqtt_init()
    Main->>MQTT: mqtt_start()
    MQTT-->>Main: Cliente iniciado
    Main->>App: app_controller_init()
    App->>Alarm: alarm_init()
    Main->>App: app_controller_start()
    App-->>Main: Tarefa principal criada
```

## Ciclo Principal

A tarefa principal roda periodicamente e executa:

1. leitura/debounce do sensor de porta;
2. detecção de abertura e fechamento;
3. cálculo do timeout adaptativo da porta;
4. leitura do NTC a cada periodo configurado;
5. atualização da janela de recuperacao termica;
6. atualização dos LEDs e buzzer;
7. publicação dos estados no MQTT quando o broker esta conectado.

```mermaid
flowchart TD
    Loop[Loop principal] --> ReadDoor[Ler porta]
    ReadDoor --> DoorChanged{Porta mudou?}
    DoorChanged -- Sim, abriu --> Open[Inicia temporizador e calcula timeout]
    DoorChanged -- Sim, fechou --> Close[Finaliza abertura e inicia recuperacao se necessario]
    DoorChanged -- Nao --> ReadTemp[Ler temperatura no periodo configurado]
    Open --> ReadTemp
    Close --> ReadTemp
    ReadTemp --> TempOk{Temperatura dentro da faixa?}
    TempOk -- Sim --> ClearTemp[Limpa alarme e recuperacao]
    TempOk -- Nao --> Recovery{Recuperacao ativa?}
    Recovery -- Sim --> Visual[LED temp pisca; buzzer temp silencioso]
    Recovery -- Nao --> StartOrAlarm[Inicia recuperacao ou ativa alarme se ja falhou]
    ClearTemp --> Outputs[Atualiza LEDs e buzzer]
    Visual --> Outputs
    StartOrAlarm --> Outputs
    Outputs --> Publish[Publica MQTT]
    Publish --> Loop
```

## Componentes De Firmware

| Componente | Papel |
| --- | --- |
| `wifi_component` | Configura NVS, netif, eventos Wi-Fi e conecta em modo station |
| `mqtt_component` | Encapsula cliente MQTT da ESP-IDF |
| `temperature_ntc` | Le ADC, calcula média, resistencia do NTC e temperatura |
| `door_sensor` | Lê o GPIO da porta com debounce |
| `adaptive_timeout` | Aprende baseline termico e estima timeout/recuperação |
| `alarm` | Controla LEDs e buzzer sem bloquear a tarefa principal |
| `app_controller` | Une sensores, regras de negocio, alarmes e MQTT |
| `app_config` | Centraliza constantes de hardware e comportamento |

## Regras De Alerta

### Porta

- porta fechada: LED vermelho da porta apagado;
- porta aberta dentro do limite: LED vermelho da porta aceso fixo;
- porta aberta alem do timeout: LED vermelho da porta piscando e buzzer alternando 1 segundo ligado/1 segundo desligado;
- abertura ou fechamento: dois bips curtos.

### Temperatura

- temperatura normal: LED azul de temperatura aceso fixo;
- temperatura fora da faixa: LED azul de temperatura piscando imediatamente;
- durante recuperacao: LED amarelo de recuperação aceso e buzzer de temperatura silencioso;
- recuperacao falhou: buzzer de temperatura passa a tocar;
- temperatura voltou ao normal: alarme termico e recuperacao sao cancelados.

### Prioridade Do Buzzer

A prioridade sonora é:

1. alerta de porta aberta além do timeout;
2. alerta de temperatura após falha de recuperação;
3. bips curtos de abertura/fechamento da porta.

Os LEDs funcionam em paralelo, então porta, temperatura e recuperação podem ser indicados visualmente ao mesmo tempo.

## MQTT

O envio MQTT acontece depois da leitura de temperatura. Se o cliente MQTT ainda não estiver conectado, o firmware apenas registra no log e segue mantendo os alertas locais.

Tópicos publicados:

- `sensor/temperatura`
- `sensor/porta`
- `sensor/alarme`
- `sensor/tempo_porta`

Tópico assinado:

- `sensor/comando`

Last Will:

- tópico: `olaf/status`
- mensagem: `offline`
