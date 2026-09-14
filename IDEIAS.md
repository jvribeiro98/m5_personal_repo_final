# 💡 Banco de Ideias e Funcionalidades — M5StickC Plus 2

Este documento reúne as propostas e conceitos selecionados para expansão do firmware `m5_personal`. Aqui são registradas as ideias aprovadas para futura implementação, com detalhes técnicos de funcionamento e viabilidade no hardware.

---

## 🎯 Ideias Selecionadas

### 1. Reconhecimento de Gestos no Ar com Rede Neural (TinyML / Edge AI)
* **Conceito:** Transformar o M5Stick em uma "varinha inteligente" capaz de identificar trajetórias tridimensionais no ar através de inferência de Machine Learning embarcada.
* **Como funciona:**
  * Coleta contínua de dados do acelerômetro e giroscópio (IMU MPU6886) em amostragem de alta frequência (~50 Hz a 100 Hz).
  * Vetor de entrada normalizado e processado por uma micro-rede neural (TensorFlow Lite for Microcontrollers ou classificador Edge Impulse) compilada em C++ para a arquitetura Xtensa LX7 do ESP32.
  * O algoritmo reconhece gestos desenhados no ar segurando o botão frontal (ex: círculos, triângulos, quadrados, letras "V", "Z", "L", acenos para cima/baixo).
  * **Inferência ultra-rápida:** menor que 20 ms, consumindo menos de 40 KB de RAM.
* **Aplicações / Disparo de Ações:**
  * Disparar comandos IR específicos (ex: desenhar "C" liga o ar no modo Cool, "V" liga TV).
  * Enviar webhooks via Wi-Fi para Home Assistant / Node-RED / automações locais.
  * Disparar atalhos BLE no computador ou smartphone.
* **Interface na tela (LCD 1.14"):**
  * Tela de "treino/desenho" mostrando a trilha do gesto em tempo real, porcentagem de confiança da IA (ex: `Gesto: CÍRCULO [98%]`) e feedback sonoro no buzzer com tom afirmativo.

---

### 2. Caçador de Rastreadores Ocultos (AirTag / SmartTag Stalker Hunter)
* **Conceito:** Dispositivo de contraespionagem e segurança pessoal que identifica rastreadores espiões (Apple AirTags, Samsung SmartTags, Tiles, Chipolo) escondidos na sua mochila, roupa ou veículo.
* **Como funciona:**
  * Varredura contínua de pacotes Bluetooth Low Energy (BLE Advertising) em modo passivo/promíscuo sem revelar a presença do M5Stick.
  * Filtro de payloads específicos:
    * **Apple Find My / AirTag:** Manufacturer ID `0x004C`, tipo `0x12` (Find My Network Advertisement), payload de 27 bytes com chave pública rotativa e status offline (>15 min longe do proprietário).
    * **Samsung SmartTag:** Manufacturer ID `0x0075`, serviço SmartThings Find.
    * **Google Find My Device / Tile:** Padrões de beacons de rastreamento Android.
  * **Algoritmo de Acompanhamento (Stalker Detection):**
    * Armazena histórico temporário em RAM de tags não-pareadas detectadas.
    * Se a mesma assinatura matemática/potência de sinal de uma tag específica for detectada continuamente em diferentes janelas de tempo (ex: acompanhando o usuário por 15+ minutos), o sistema dispara o alarme.
* **Aplicações / Recursos:**
  * **Modo Sentinela Silencioso:** Fica rodando em baixo consumo com a tela apagada no porta-luvas do carro ou mochila.
  * **Alerta Visual & Sonoro:** Pisca o LED vermelho frontal, toca sequência de bipes no buzzer e acende a tela com alerta: `[ALERTA: AIRTAG DETECTADA]`.
  * **Radar de Proximidade (Caça ao Alvo):** Exibe um medidor de intensidade de sinal (RSSI em dBm) com barra gráfica e som com cadência variável (tipo contador Geiger) para guiar você fisicamente até onde o rastreador foi escondido.

---

### 3. Detector de Skimmers de Cartão de Crédito e Caixas Adulterados (Infosec)
* **Conceito:** Scanner de bolso para proteção financeira contra clonagem de cartão de crédito/débito em caixas eletrônicos (ATMs) e bombas de combustível.
* **Como funciona:**
  * Quadrilhas instalam "chupa-cabras" (skimmers) para roubar a trilha magnética do cartão e o PIN digitado. Para não abrir a máquina para recuperar os dados, utilizam módulos seriais Bluetooth baratos e populares (HC-05, HC-06, RN-42, BK3231).
  * O M5Stick realiza varredura ativa e passiva nos protocolos Bluetooth Classic e Low Energy (BR/EDR / BLE):
    * **Assinaturas de Nomes Padrão:** Identifica nomes comuns deixados pelos criminosos (ex: `HC-05`, `HC-06`, `RN42`, `SPP-CA`, `NULL`, `FreeBit`, `RN-BT`).
    * **Prefixos OUI / Endereços MAC:** Mapeia endereços MAC associados a fabricantes chineses de módulos UART baratos comumente empregados nesses dispositivos ilícitos (ex: `00:14:03`, `20:16:12`).
    * **Perfil de Serviço SPP (Serial Port Profile):** Detecta se há canal serial aberto aguardando conexão em locais onde só deveria haver transações fechadas.
* **Aplicações / Recursos:**
  * **Verificação em 1 Toque:** Ao parar no posto ou entrar na agência, aperta o botão para varrer o raio de 5 a 10 metros.
  * **Alerta Crítico:** Se detectar um módulo suspeito com alta intensidade de sinal (indicando proximidade imediata da máquina), a tela acende em vermelho com `[PERIGO: SKIMMER DETECTADO]` e bipe de alerta antes de você inserir o cartão.
  * **Modo Radar:** Permite ver a intensidade do sinal (via RSSI) para identificar qual máquina específica está adulterada.

---

### 4. Canivete Suíço de Diagnóstico de Bancada e Eletrônica (Osciloscópio, Roda Fônica & Gerador de Sinais)
* **Conceito:** Transformar o M5Stick em uma ferramenta autônoma de bancada para diagnóstico de circuitos, injeção automotiva e testes de atuadores diretamente pelos pinos HAT/Grove.
* **Como funciona:**
  * **Mini-Osciloscópio Digital Rápido (Pino G36 / ADC DMA):**
    * Leitura contínua com buffer DMA a até 200 kSPS (quilosamples por segundo).
    * Algoritmo de trigger por borda (subida/descida) ajustável para congelar formas de onda na tela.
    * Cálculo automático em tempo real: Tensão Pico a Pico ($V_{pp}$), Tensão Média ($V_{rms}$), Frequência ($Hz$) e Duty Cycle ($%$).
  * **Simulador de Roda Fônica Automotiva (Pino G26):**
    * Gera trens de pulso simulando sensores indutivos ou Hall com falha de dente para testar módulos de injeção (ECU) na bancada sem precisar girar o motor real.
    * Padrões selecionáveis: **60-2** (padrão Bosch / linha nacional e importada), **36-1** (padrão Ford/Toyota), com rotação ajustável de 100 RPM a 12.000 RPM.
  * **Gerador PWM de Potência e Frequência Variável:**
    * Frequência configurável de 1 Hz até 100 kHz com precisão de microssegundos e duty-cycle de 0% a 100%.
    * Utilizado para acionar e testar bicos injetores, bombas elétricas, válvulas PWM (EGR, marcha lenta, solenoide de turbo) e servos de modelismo.
  * **Gerador de Tensão DC Analógica (DAC 8 bits no G26):**
    * Injeta tensões de 0.0V a 3.3V com passos de 0.05V para simular sensores TPS (posição de borboleta), MAP (pressão) ou temperatura.
* **Aplicações / Recursos:**
  * Permite testar atuadores, verificar se módulos estão respondendo e diagnosticar falhas elétricas em campo sem precisar de bancada de testes pesada.

---

## 🔬 Novas Ideias em Avaliação

*(Novas propostas e pesquisas serão adicionadas aqui para análise e seleção)*
