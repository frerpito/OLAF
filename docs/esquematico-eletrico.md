# Esquematico Eletrico E Ligacoes

Este documento descreve as conexoes eletricas usadas pelo firmware atual do OLAF.

![Esquematico eletrico do OLAF](esquematico-eletrico.svg)

## Resumo Dos GPIOs

| Funcao | GPIO/Canal | Direcao | Observacao |
| --- | --- | --- | --- |
| Sensor de porta E18-D80NK | GPIO5 | Entrada digital | Nivel configuravel por `OLAF_DOOR_CLOSED_LEVEL` |
| Sensor NTC 10K | ADC1_CH3 | Entrada analogica | Ponto central do divisor de tensao |
| LED vermelho da porta | GPIO35 | Saida digital | Usar resistor em serie |
| LED azul de temperatura | GPIO42 | Saida digital | Usar resistor em serie |
| LED amarelo de recuperacao | GPIO2 | Saida digital | Usar resistor em serie |
| Buzzer ativo | GPIO36 | Saida digital | Usar driver se consumir corrente alta |

## Sensor NTC 10K

O NTC usa um divisor de tensao com resistor fixo de 10 kOhm.

```text
3.3 V
  |
  +-- Resistor fixo 10 kOhm
  |
  +---------- ADC1_CH3
  |
  +-- NTC 10K
  |
 GND
```

Parametros relacionados em `components/app_config/app_config.h`:

| Constante | Valor atual | Funcao |
| --- | --- | --- |
| `OLAF_NTC_ADC_UNIT` | `ADC_UNIT_1` | Unidade ADC usada |
| `OLAF_NTC_ADC_CHANNEL` | `ADC_CHANNEL_3` | Canal ADC do NTC |
| `OLAF_NTC_R0_OHM` | `10000.0f` | Resistencia nominal do NTC |
| `OLAF_NTC_FIXED_R_OHM` | `10000.0f` | Resistor fixo do divisor |
| `OLAF_NTC_T0_C` | `25.0f` | Temperatura de referencia |
| `OLAF_NTC_BETA` | `3950.0f` | Constante Beta inicial |

Se a leitura ficar invertida ou fora do esperado, confira:

- se o resistor fixo esta ligado ao 3.3 V;
- se o NTC esta ligado ao GND;
- se o ponto central esta no canal ADC correto da placa;
- se o valor Beta corresponde ao NTC real usado.

## Sensor De Porta E18-D80NK

Ligacao recomendada:

| Pino do sensor | Ligacao |
| --- | --- |
| VCC | Alimentacao conforme o modulo utilizado |
| GND | GND comum com o ESP32 |
| OUT | GPIO5 do ESP32, com adequacao de nivel se necessario |

Atencao: os GPIOs do ESP32 nao sao tolerantes a 5 V. Se a saida do E18-D80NK estiver em 5 V, use divisor resistivo, conversor de nivel logico ou interface com transistor/optoacoplador antes do GPIO5.

O nivel logico considerado como porta fechada fica em:

```c
#define OLAF_DOOR_CLOSED_LEVEL 0
```

Se o comportamento ficar invertido, altere esse valor para `1`.

## LEDs

Cada LED deve usar resistor em serie de 220 Ohm ou 330 Ohm.

Ligacao padrao:

```text
GPIO ---- resistor ---- anodo LED
                         catodo LED ---- GND
```

| LED | Cor recomendada | GPIO | Significado |
| --- | --- | --- | --- |
| Porta | Vermelho | GPIO35 | Porta aberta ou porta aberta alem do timeout |
| Temperatura | Azul | GPIO42 | Temperatura normal ou fora da faixa |
| Recuperacao | Amarelo | GPIO2 | Janela de recuperacao termica ativa |

Comportamento esperado:

| LED | Apagado | Aceso fixo | Piscando |
| --- | --- | --- | --- |
| Vermelho da porta | Porta fechada | Porta aberta dentro do tempo permitido | Porta aberta alem do timeout |
| Azul de temperatura | Nao usado em operacao normal | Temperatura dentro da faixa aceitavel | Temperatura fora da faixa aceitavel |
| Amarelo de recuperacao | Sem recuperacao ativa | Recuperacao ativa com temperatura ainda fora da faixa | Nao usado pelo firmware atual |

## Buzzer

O firmware considera um buzzer ativo, acionado por nivel alto no GPIO36.

Para buzzer ativo de baixa corrente e 3.3 V:

```text
GPIO36 ---- terminal positivo do buzzer
GND   ---- terminal negativo do buzzer
```

Para buzzer de 5 V ou maior corrente, use transistor ou MOSFET:

```text
GPIO36 -- resistor -- base/gate do transistor
GND comum entre ESP32 e fonte do buzzer
Buzzer alimentado pela fonte adequada
Transistor/MOSFET chaveando o lado negativo da carga
```

Nao ligue cargas de alta corrente diretamente no GPIO do ESP32.

Comportamento sonoro esperado:

| Situacao | Som |
| --- | --- |
| Porta abriu | Dois bips curtos |
| Porta fechou | Dois bips curtos |
| Porta ficou aberta alem do timeout | Alerta alternado, 1 segundo ligado e 1 segundo desligado, ate fechar a porta |
| Temperatura saiu da faixa, mas ainda esta em recuperacao | Sem som |
| Recuperacao falhou e temperatura continua fora da faixa | Alerta alternado por ciclos baseados no tempo de recuperacao |

Prioridade do buzzer:

1. alerta de porta aberta alem do timeout;
2. alerta de temperatura apos falha de recuperacao;
3. bips curtos de abertura/fechamento.

## Alimentacao E GND

- O ESP32, sensores, LEDs e buzzer devem compartilhar referencia de GND.
- Use 3.3 V para sinais que entram diretamente no ESP32.
- Se algum modulo exigir 5 V, garanta que o sinal de saida dele seja adaptado para 3.3 V antes de entrar no ESP32.

## Lista De Verificacao Da Montagem

Antes de gravar o firmware:

- conferir curto entre 3.3 V e GND;
- conferir polaridade dos LEDs;
- conferir resistor em serie em cada LED;
- conferir divisor de tensao do NTC;
- conferir nivel de saida do sensor E18-D80NK;
- conferir se o buzzer e ativo;
- conferir GND comum entre todos os modulos.
