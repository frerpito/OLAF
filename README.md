# OLAF — ESP32-S3 + ESP-IDF

Firmware desenvolvido para o projeto **OLAF — Observador Local de Ambientes
Frigorificados**, utilizando uma ESP32-S3 e o framework ESP-IDF.

## Objetivo

O sistema realiza o monitoramento preventivo de uma câmara frigorífica por
meio da leitura da temperatura interna e da identificação do estado da porta.

A ESP32-S3 realiza o processamento local das informações e controla os
alertas luminoso e sonoro independentemente da disponibilidade da conexão
Wi-Fi.

## Hardware

O protótipo utiliza:

- ESP32-S3;
- termistor NTC 10 kΩ;
- sensor fotoelétrico E18-D80NK;
- LED;
- buzzer piezoelétrico ativo.

## Funcionalidades

O firmware implementa:

- leitura periódica da temperatura;
- média de múltiplas amostras do ADC;
- conversão da resistência do NTC para temperatura;
- detecção de porta aberta e fechada;
- filtro temporal do sensor da porta;
- contagem do tempo de abertura;
- LED indicador de porta aberta;
- LED piscante em condição crítica;
- buzzer para alerta;
- alarme independente por temperatura;
- cálculo de temperatura média de referência da câmara;
- monitoramento da recuperação térmica após fechamento da porta;
- cálculo adaptativo do tempo máximo permitido de abertura.

## Estrutura

```text
olaf_espidf/
│
├── CMakeLists.txt
├── sdkconfig.defaults
├── README.md
│
└── main/
    ├── CMakeLists.txt
    ├── app_main.c
    ├── app_config.h
    │
    ├── app_controller.c
    ├── app_controller.h
    │
    ├── door_sensor.c
    ├── door_sensor.h
    │
    ├── temperature_ntc.c
    ├── temperature_ntc.h
    │
    ├── alarm.c
    ├── alarm.h
    │
    ├── adaptive_timeout.c
    └── adaptive_timeout.h
```

## Organização do firmware

### app_main

Ponto de entrada da aplicação.

Inicializa o controlador principal e cria a tarefa responsável pelo
monitoramento da câmara.

### app_controller

Responsável pela lógica principal do sistema.

Integra:

- sensor da porta;
- sensor de temperatura;
- temporização;
- timeout adaptativo;
- LED;
- buzzer.

### door_sensor

Responsável pela leitura do E18-D80NK.

Também aplica um filtro temporal para evitar alterações falsas do estado da
porta causadas por oscilações rápidas do sinal.

### temperature_ntc

Responsável pela aquisição do NTC utilizando o ADC da ESP32-S3.

Realiza múltiplas amostras do ADC, calcula a média e converte o resultado
para resistência e temperatura.

### alarm

Responsável pelo LED e pelo buzzer.

Existem três estados:

```text
NORMAL
LED apagado
Buzzer desligado

PORTA ABERTA
LED aceso
Buzzer desligado

CRÍTICO
LED piscando
Buzzer ligado
```

### adaptive_timeout

Responsável pelo cálculo do tempo máximo permitido para a porta permanecer
aberta.

O limite é calculado considerando:

1. temperatura atual da câmara;
2. temperatura média de referência;
3. histórico do tempo de recuperação térmica.

A ideia geral utilizada é:

```text
timeout =
    timeout_base
    × fator_temperatura
    × fator_recuperacao
```

Quanto mais próxima a temperatura estiver do limite crítico, menor será o
tempo permitido de abertura.

Da mesma forma, se a câmara apresentar recuperação térmica lenta após
aberturas anteriores, o sistema reduzirá o limite utilizado nas próximas
aberturas.

## Funcionamento

Fluxo simplificado:

```text
              ┌─────────────────┐
              │  PORTA FECHADA  │
              └────────┬────────┘
                       │
                 monitora NTC
                       │
                       ▼
              calcula temperatura
                 média/baseline
                       │
                 porta abriu?
                       │
                      SIM
                       ▼
              ┌─────────────────┐
              │   PORTA ABERTA  │
              └────────┬────────┘
                       │
              calcula timeout
                 adaptativo
                       │
                  LED aceso
                       │
                       ▼
              tempo > timeout?
                  │         │
                 NÃO       SIM
                  │         │
                  │         ▼
                  │    LED piscando
                  │    buzzer ligado
                  │
                  ▼
              porta fechou?
                       │
                      SIM
                       ▼
              mede recuperação
                  térmica
                       │
                       ▼
              atualiza histórico
                       │
                       ▼
               PORTA FECHADA
```

## Timeout adaptativo

O documento do projeto determina que o tempo máximo de abertura seja
variável de acordo com a temperatura e com o comportamento térmico observado
na câmara.

Como o documento não define uma equação matemática específica, o firmware
implementa uma política configurável.

O valor inicial utilizado é:

```text
Timeout base = 300 segundos
```

O resultado é limitado por:

```text
Timeout mínimo = 30 segundos
Timeout máximo = 420 segundos
```

Esses valores são parâmetros iniciais e devem ser ajustados durante os
ensaios do protótipo.

## Recuperação térmica

Quando a porta é fechada, o sistema continua monitorando a temperatura.

Por exemplo:

```text
Temperatura normal antes da abertura:

2.3 °C

Após abertura e fechamento:

4.1 °C

Recuperação:

4.1
3.8
3.4
3.0
2.7
2.5
2.4
```

Quando a temperatura retorna para próximo da temperatura média anterior, o
firmware calcula quanto tempo a recuperação levou.

Esse valor passa a fazer parte do histórico da câmara.

Uma câmara que demora mais para recuperar recebe um timeout menor nas
aberturas futuras.

## Compilação

Configure o alvo:

```bash
idf.py set-target esp32s3
```

Compile:

```bash
idf.py build
```

Grave na ESP32-S3:

```bash
idf.py -p COMx flash
```

Para gravar e abrir o monitor serial:

```bash
idf.py -p COMx flash monitor
```

Substitua `COMx` pela porta correspondente à ESP32-S3.

## Configuração dos GPIOs

Os GPIOs estão centralizados em:

```text
main/app_config.h
```

Isso permite alterar os pinos sem modificar os módulos individuais.

Os valores fornecidos inicialmente são apenas uma configuração de
desenvolvimento e devem ser conferidos com o pinout da placa ESP32-S3
utilizada no protótipo.

## NTC

O firmware considera inicialmente:

```text
R0   = 10 kΩ
T0   = 25 °C
Beta = 3950 K
```

O valor Beta deve ser confirmado de acordo com o termistor MF52 utilizado.

Para melhorar a precisão, recomenda-se realizar calibração utilizando um
termômetro de referência.

## E18-D80NK

A interface elétrica do E18-D80NK deve ser verificada antes da conexão com
a ESP32-S3.

Os GPIOs da ESP32-S3 trabalham em 3,3 V e não devem receber diretamente
tensões superiores ao limite permitido pelo microcontrolador.

Dependendo da versão do E18-D80NK utilizada, pode ser necessária uma
interface de adaptação de nível.

## Próximas etapas

A arquitetura permite posteriormente adicionar:

- Wi-Fi;
- cliente MQTT;
- publicação da temperatura;
- publicação do estado da porta;
- publicação do timeout;
- publicação dos alertas;
- integração com Grafana/dashboard;
- armazenamento de parâmetros em NVS.