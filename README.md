# Projeto Embarcado: *Climário*

Desenvolvimento de um terrário que simula condições climáticas em diferentes modos: Ensolarado, Chuvoso, Tempestuoso, e o modo Local que replica as consições climáticas da sua localização.

![ENSOL](https://github.com/Moskbr/Climario/blob/main/Imagens%20e%20Videos/Modo_Ensolorado.jpeg)

### Objetivo

Fomentar o interesse de jovens à ingressar na área da tecnologia, além de promover o nome da instituição na comunidade por meio de um projeto de Sistemas Embarcados em exposição na Usina do Conhecimento.

## Descrição do projeto

### Componentes utilizados

#### Parte eletrônica:

- ESP32 WiFi
- Sensor DHT22
- Sensor BMP180
- Fita LEDs RGB
- Bomba D'água

#### Estrutural:

- Terrário de Vidro
- Gotejador feito na impressora 3D

### Funcionalidade

O sistema é capaz de simular as condições climáticas do modo em que estiver, acionando equipamentos externos: reservatório de água e bomba para simular chuva; LEDs RGB para simular céu ensolarado ou com tempestades.

A obtenção dos dados climáticos no modo Local são feitos por meio dos sensores de temperatura, pressão e humidade, e outros dados como probabilidade de chuva e velocidade do vento, são por meio da API [OpenMeteo](https://open-meteo.com/).

Cada modo de clima mantém suas respectivas condições climáticas, exceto o modo Local.

O microcontrolador faz a leitura dos sensores periodicamente (a cada 2 minutos), armazenando esses dados para que posteriormente, a cada 20 minutos, faça a média das leituras obtidas e mostra em um servidor HTTP simples, além do modo atual. No servidor pode ser trocado o modo com um delay de 2 segundos para uma nova troca, e foi adaptado a partir de um projeto já existente do usuário "Enjoy-Mechatronics" disponível no Repositório: [link](https://github.com/Enjoy-Mechatronics/ESP32-DHT-Webserver).

### Desempenho

O sistema atualiza as medições e condições de clima dentro do terrário a cada 20 minutos.

### Especificações Técnicas

- #### Tamanho e peso:
    O terrário tem dimensões 15x15x15cm, e conta com um gotejador acoplado na saída da bomba d'água.

- #### Custo de fabricação:
    Máximo 250 reais.

- #### Consumo:
    O microcontrolador utilizado é o ESP32, com alimentação USB de 2,5-3W.

### Documentação

Este repositório conta com imagens e vídeos do projeto funcionando, e um relatório, além do código fonte.
