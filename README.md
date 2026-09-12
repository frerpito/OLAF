# OLAF — Observador Local de Ambientes Frigorificados

Sistema IoT para monitoramento preventivo de câmaras frigoríficas utilizando ESP32, sensores de temperatura e detecção de abertura de porta.

Grupo Lamparina

## Sobre o projeto

O OLAF — Observador Local de Ambientes Frigorificados é um sistema embarcado desenvolvido para realizar o monitoramento preventivo de câmaras frigoríficas, identificando situações que possam comprometer a conservação dos produtos armazenados.

A solução monitora simultaneamente a temperatura interna, o estado da porta e o tempo em que ela permanece aberta, permitindo detectar uma possível causa de alteração térmica antes que seus efeitos se agravem.

O processamento é realizado localmente por um ESP32, responsável por analisar os dados e acionar alertas visuais e sonoros, mesmo sem conexão Wi-Fi. Além disso, as informações são disponibilizadas em um dashboard para acompanhamento remoto.

O objetivo é proporcionar uma resposta mais rápida e preventiva a situações anormais, contribuindo para a conservação dos produtos e para o funcionamento eficiente do ambiente frigorificado.

## Arquitetura do sistema

O OLAF possui uma arquitetura dividida em quatro camadas:

**Sensoriamento:** o NTC 10K MF52 realiza a medição da temperatura, enquanto o E18-D80NK identifica o estado da porta.

**Processamento:** o ESP32 realiza a leitura dos sensores, contabiliza o tempo de abertura da porta e determina o acionamento dos alertas.

**Comunicação:** os dados são enviados via Wi-Fi utilizando o protocolo MQTT e um broker para distribuição das mensagens.

**Supervisão:** os dados são disponibilizados em um dashboard para visualização da temperatura, estado da porta, histórico e alertas.

## Fluxo de funcionamento

![Diagrama de arquitetura do OLAF](docs/fluxo-projeto.png)

Os alertas locais são processados diretamente pelo ESP32 e, portanto, não dependem da conexão Wi-Fi para funcionar.

## Tecnologias utilizadas

### Hardware

Teconologias utilizadas:

- hardware;
- firmware;
- comunicação;
- monitoramento;
- desenvolvimento;

| Componente | Função |
| --- | --- |
| ESP32 | Processamento local, leitura dos sensores e comunicação Wi-Fi |
| NTC 10K 3 mm MF52 | Medição da temperatura interna |
| E18-D80NK | Detecção do estado da porta |
| Buzzer piezoelétrico | Alerta sonoro local |
| LED | Indicação visual de alerta |
| Bateria Li-ion 18650 | Alimentação de backup |

### Firmware

- C/C++
- ESP-IDF
- FreeRTOS
- GPIO
- ADC
- Wi-Fi

### Desenvolvimento

- Visual Studio Code
- Extensão Espressif IDF
- Git
- GitHub

## Pré-requisitos

Antes de executar o projeto, certifique-se de possuir:

- Visual Studio Code;
- extensão Espressif IDF instalada;
- ESP-IDF configurado;
- Git;
- cabo USB com suporte à transferência de dados;
- placa ESP32 compatível;
- drivers USB necessários para reconhecimento da placa.

Para as funcionalidades remotas também serão necessários:

- acesso a uma rede Wi-Fi 2.4 GHz;
- broker MQTT;
- ambiente Grafana configurado.

## Configuração do ESP-IDF no VS Code

### 1. Instalar o Visual Studio Code

Instale o Visual Studio Code no sistema operacional utilizado para desenvolvimento.

### 2. Instalar a extensão ESP-IDF

No VS Code, abra:

Extensions → Pesquisar "ESP-IDF"

Instale a extensão:

Espressif IDF

## Executando o projeto

### 1. Clonar o repositório

```bash
git clone <URL_DO_REPOSITORIO>
cd <NOME_PROJETO>
```

### 2. Abrir no Visual Studio Code

No powershell ou cmd, digite “code .” para abrir o projeto no VSCode. Você também pode abrir o VSCode, ir em arquivo -> abrir pasta e selecionar a pasta do projeto.

### 3. Selecionar o dispositivo ESP32

Selecione o modelo de ESP32 utilizado no projeto.

## Compilação

Pelo terminal configurado do ESP-IDF:

```bash
idf.py build
```

## Gravação no ESP32

Conecte o ESP32 ao computador através do cabo USB.

Pelo terminal:

```bash
idf.py -p PORTA flash
```

## Monitor Serial

Para acompanhar os logs gerados pelo ESP32:

```bash
idf.py -p PORTA monitor
```

Também é possível compilar, gravar e abrir o monitor em sequência:

```bash
idf.py -p PORTA flash monitor
```

## Equipe

Grupo Lamparina

| Integrante |
| --- |
| Francisco Guilherme Cesário Alcântara |
| Guilherme Viana Batista |
| Pedro Henrique Bezerra Simeão |
| Raissa Karoliny da Silva Rodrigues |
