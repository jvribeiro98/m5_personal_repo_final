# 🎮 M5 Personal — Universal Multi-IR, Wi-Fi Remote, Air Mouse BLE & AI Voice Assistant (M5StickC Plus2)

[![PlatformIO](https://img.shields.io/badge/PlatformIO-Compatible-FF8C00?style=for-the-badge&logo=platformio&logoColor=white)](https://platformio.org/)
[![Arduino ESP32](https://img.shields.io/badge/Arduino_ESP32-3.3.8-00979D?style=for-the-badge&logo=arduino&logoColor=white)](https://espressif.github.io/arduino-esp32/)
[![M5Unified](https://img.shields.io/badge/M5Unified-0.2.19+-E60012?style=for-the-badge)](https://github.com/m5stack/M5Unified)
[![NimBLE](https://img.shields.io/badge/NimBLE_Arduino-2.2+-0080FF?style=for-the-badge)](https://github.com/h2zero/NimBLE-Arduino)
[![IRremoteESP8266](https://img.shields.io/badge/IRremoteESP8266-v2.9+-107C41?style=for-the-badge)](https://github.com/crankyoldgit/IRremoteESP8266)
[![Web Flash](https://img.shields.io/badge/Web_Flash-ESP_Web_Tools-4285F4?style=for-the-badge&logo=googlechrome&logoColor=white)](https://jvribeiro98.github.io/m5_personal_repo_final/)

> **Firmware multifuncional em C++ para M5StickC Plus2 (ESP32-PICO-V3-02) integrando Assistente de Voz com modo Alexa mãos-livres ("Ei M5"), ponte com IA persistente Antigravity (agy), Air Mouse BLE por giroscópio sem deriva, controle remoto infravermelho universal com disparo direto no hardware, interface fluida em double-buffering (padrão Bruce/CatHack), bateria estabilizada por filtro IIR, WebUI responsiva, relógio RTC com previsão do tempo e gravação via navegador.**

---

## ✨ Principais Funcionalidades

### 🎙️ 1. Agente IA — Modo Alexa Mãos-Livres ("Ei M5") & Push-to-Talk
- **Hands-free total (Modo Alexa):** O microfone interno (SPM1423 via I2S a 16kHz) monitora o áudio em background. Ao dizer *"Ei M5"*, *"M5"* ou *"Alexa"*, o comando é capturado e enviado automaticamente.
- **Pré-buffer inteligente no PSRAM:** Buffer circular que impede que as primeiras sílabas da fala sejam cortadas.
- **Detecção de Silêncio (Cooldown 2.0s):** Finaliza a gravação e envia o áudio assim que o usuário para de falar, sem exigir interação física.
- **Descarte silencioso de conversas paralelas:** Áudios sem a palavra de ativação são descartados silenciosamente pelo PC (`{"type": "IGNORE"}`), sem interromper a tela atual.
- **Modo Push-To-Talk (PTT):** Alternativa com clique único em **[A]** para falar e outro para enviar, com suporte a gravações de até 30 segundos no PSRAM.
- **Persistência Total da Resposta:** A resposta da IA fica fixa na tela pelo tempo que o usuário desejar ler (sem auto-dismiss). O M5 continua escutando em segundo plano e, ao receber um novo comando, **a nova resposta sobe e substitui a anterior diretamente**.
- **Rolagem rápida de textos longos:** Botões **[B]** (rolar para baixo) e **[C]** (rolar para cima) com paginação suave.
- **Controle Físico de Hardware IR via Voz:** Comandos como *"Ei M5, desligue o ar condicionado"* ou *"Ei M5, aumente o volume da TV"* são interpretados e disparados **diretamente no LED infravermelho de hardware (GPIO 19)** do M5Stick.

### 🔋 2. Estabilização Isolada do Medidor de Bateria
- **Filtro Digital Passa-Baixa (IIR) + Oversampling de 4 leituras:** Elimina 100% da oscilação do conversor analógico (ADC no GPIO 38) provocada por picos transitórios de consumo do processador e barramento SPI do display.
- **Amostragem desacoplada do redesenho (Rate Limiting a cada 3s):** A porcentagem permanece **imóvel e estável** durante a navegação e rolagem de menus.
- **Descarga Monótona com Histerese:** Em uso na bateria, a porcentagem nunca salta para cima devido a ruídos elétricos. Ao plugar o cabo USB, o sistema detecta o carregamento e sobe o nível progressivamente.

### 🖥️ 3. Interface Fluida sem Flicker (Padrão Bruce / CatHack)
- **Double-Buffering com `M5Canvas`:** Renderização gráfica atômica a 60 FPS alocada no PSRAM (`uiCanvas.pushSprite(0, 0)`), eliminando o efeito de piscada preta ("mudando de foto") entre quadros e animações.
- **Orientação Landscape unificada (240x135):** Layout ergonômico consistente em todos os menus e ferramentas.
- **Cyber Boot Intro:** Tela de inicialização futurista com logotipo estilizado, barra de carregamento e chime sonoro polifônico.

### 🖱️ 4. Air Mouse BLE (Bluetooth Low Energy HID)
- Controle do cursor do mouse no PC ou Celular pelo movimento do punho no ar.
- **Zero-Drift:** Calibração automática de repouso em alta precisão ao entrar na tela e auto-zero contínuo.
- **Click Stabilization:** Amortecimento automático do cursor durante o clique mecânico para evitar desvios acidentais do alvo.
- **Orientação Portrait (135x240):** Uso ergonômico natural em pé, com o botão grande sob o polegar e mira gráfica em tempo real.
- Compatível nativamente com Windows, Android, macOS, Linux, iOS e iPadOS sem nenhum aplicativo ou driver extra.

### ❄️ 5. Controle Remoto de Ar-Condicionado (IR)
- Suporte completo a Samsung (protocolos 114/168-bit), Midea (48-bit) e modelos Midea Antigo (COOLIX).
- Ajuste de temperatura, modo (Frio/Auto), ventilação, Turbo e Swing.

### 📺 6. Controle de Televisores (IR)
- TV Samsung (NEC 32-bit), TV LG (32-bit) e TV TCL com teclado de navegação e atalhos rápidos.

### 📶 7. Gerenciador Wi-Fi & WebUI Local
- Armazenamento persistente (NVS) de até 10 redes com prioridade por sinal (RSSI).
- WebServer local com interface responsiva para controlar IR e Wi-Fi direto pelo smartphone ou computador.

### ⏰ 8. Relógio RTC & Previsão do Tempo
- Sincronização automática de horário via NTP e clima em tempo real via Open-Meteo com suporte a múltiplos estilos de watchface (Cyber HUD, Big Neon, Matrix).

### 🐎 9. Módulo Team (Treinamento & Contador)
- Contador de gado e gerenciamento de passadas de cavalos com histórico de treinos.

---

## 🏗️ Arquitetura do Sistema

```mermaid
graph TD
    subgraph "Hardware (M5StickC Plus2)"
        MCU["ESP32-PICO-V3-02 (240MHz, 8MB Flash, 2MB PSRAM)"]
        MIC["Microfone I2S (SPM1423 - GPIO 0/34)"]
        IMU["Sensor IMU 6-Eixos (MPU6886 / BMI270)"]
        IR_LED["Emissor IR Físico (GPIO 19)"]
        TFT["Display LCD TFT 1.14\" (ST7789v2)"]
        BTNS["Botões Físicos (A frontal, B lateral, C power)"]
        BAT_ADC["Medidor ADC Bateria (GPIO 38) + Filtro IIR"]
    end

    subgraph "Módulos de Controle Local"
        VOICE_CLIENT["🎙️ Cliente de Voz (VOX + Double-Buffer Canvas)"]
        BLE_MOUSE["🖱️ Air Mouse BLE (NimBLE HID Driver)"]
        IR_AC["❄️ Ar-Condicionado (Samsung, Midea, Coolix)"]
        IR_TV["📺 Televisores (Samsung, LG, TCL)"]
        TEAM["🐎 Contador & Treino Team Penning"]
    end

    subgraph "PC Bridge & IA"
        BRIDGE["🐍 m5_agent_bridge.py (HTTP :5000 / Serial COM)"]
        AGY["🧠 Antigravity CLI (agy) - Sessão Persistente"]
        STT["🗣️ Reconhecimento de Voz & Wake-Word"]
    end

    MIC --> VOICE_CLIENT
    VOICE_CLIENT <-->|HTTP / Serial| BRIDGE
    BRIDGE <--> STT
    BRIDGE <--> AGY
    BRIDGE -->|JSON IR Tag| VOICE_CLIENT
    VOICE_CLIENT --> IR_LED

    IMU --> BLE_MOUSE
    BTNS --> MCU
    BAT_ADC --> MCU
    MCU --> TFT
    IR_AC --> IR_LED
    IR_TV --> IR_LED
```

---

## 🕹️ Mapeamento de Botões Físicos

### Modo Geral (Menus do Sistema)
| Botão | Toque Rápido | Segurar (> 1.2s) |
| :--- | :--- | :--- |
| **Botão A (Grande Frontal)** | Selecionar / Confirmar / Executar | — |
| **Botão B (Lateral)** | Próximo item do menu / Incrementar | Voltar ao menu anterior |
| **Botão C / PWR (Topo)** | Item anterior do menu / Decrementar | Desligar (*Power Off*) |

### Modo Agente IA (Visor de Resposta & Escuta)
| Botão | Toque Rápido | Segurar (> 1.2s) |
| :--- | :--- | :--- |
| **Botão A (Frontal)** | Forçar início de fala ou envio imediato (PTT) | — |
| **Botão B (Lateral)** | **Rolar texto para BAIXO** (em resposta) ou Alternar Modo (Alexa/PTT) | — |
| **Botão C (Topo)** | **Rolar texto para CIMA** (em resposta) | Voltar ao Menu Principal |

### Modo Air Mouse BLE
| Botão | Ação | Função no Cursor |
| :--- | :--- | :--- |
| **Botão A (Grande Frontal)** | 1 Clique | **Clique Esquerdo** (Bolinha pisca verde) |
| **Botão A (Grande Frontal)** | Pressionar e Segurar | **Arrastar / Segurar botão esquerdo** |
| **Botão B (Lateral)** | 1 Clique ou Segurar | **Clique Direito** (Bolinha pisca ciano) |
| **Botão C / PWR (Topo)** | 1 Clique | Voltar ao Menu Principal |
| **Botão C / PWR (Topo)** | Segurar (> 2s) | **Recalibrar Repouso** (Zera qualquer desvio) |

---

## 🐍 Inicialização da Ponte de Voz no PC (Agente IA)

Para utilizar o Agente IA com o modo Alexa mãos-livres ou integração com o PC:

1. Conecte o M5StickC Plus2 na mesma rede Wi-Fi do seu computador ou via cabo USB na porta serial (`COM3`).
2. Instale as dependências no Python (caso ainda não possua):
   ```bash
   pip install pyserial SpeechRecognition numpy
   ```
3. Inicie a ponte executando o script dedicado:
   ```bash
   python m5_agent_bridge.py
   ```
   *(Ou execute diretamente o atalho `iniciar_agente_m5.bat`)*
4. Entre na tela **Agente IA** no menu do M5Stick e fale:
   - *"Ei M5, que horas são?"*
   - *"Ei M5, desligue o ar condicionado"* (dispara o comando físico de IR para Ar Samsung no GPIO 19).
   - *"Ei M5, abra o projeto no VS Code"* (executa comandos e automações diretamente no seu PC via Antigravity).

---

## 📡 Instalação Direta pelo Navegador (Web Flash)

Você pode instalar ou atualizar o firmware diretamente no seu M5StickC Plus2 sem precisar de Arduino IDE ou compiladores:

1. Acesse: **[jvribeiro98.github.io/m5_personal_repo_final](https://jvribeiro98.github.io/m5_personal_repo_final/)**
2. Conecte o M5StickC Plus2 via cabo USB ao computador (no Google Chrome ou Microsoft Edge).
3. Clique em **"Conectar e Instalar"** e selecione a porta serial (ex: `COM3` ou `/dev/ttyUSB0`).
4. A gravação é realizada automaticamente através do binário unificado compilado pelo GitHub Actions.

---

## 🚀 Compilação e Gravação Manual

### Configuração no Arduino IDE
- **Placa:** `M5StickC-Plus2` (ou `ESP32-PICO-DevKit`)
- **CPU Frequency:** `240MHz`
- **Flash Size:** `8MB (64Mb)`
- **Partition Scheme:** `Default 8MB with spiffs`
- **PSRAM:** `Enabled`
- **Bibliotecas Necessárias:**
  - `M5Unified` (`>= 0.2.19`)
  - `M5GFX` (`>= 0.2.26`)
  - `IRremoteESP8266` (`>= 2.9.0`)
  - `NimBLE-Arduino` (`>= 2.2.3`)

### Compilação via Arduino CLI
```bash
arduino-cli compile --fqbn esp32:esp32:m5stack_stickc_plus2:UploadSpeed=1500000,CPUFreq=240,FlashFreq=80,FlashMode=qio,FlashSize=8M,PartitionScheme=default_8MB,DebugLevel=none,PSRAM=enabled firmware
```

### Gravação via Arduino CLI / esptool
```bash
arduino-cli upload -p COM3 --fqbn esp32:esp32:m5stack_stickc_plus2 firmware
```

---

## ⚙️ Estrutura do Repositório

```text
├── firmware/
│   ├── m5_personal.ino       # Firmware principal em C++ (Modo Alexa, IR, Bateria IIR, Canvas)
│   ├── M5StickBleMouse.h     # Driver NimBLE BLE HID Mouse otimizado
│   ├── MouseCalibration.h   # Algoritmo de calibração de repouso e zero-drift
│   └── GestureAI.h           # Reconhecedor de gestos IMU 3D
├── m5_agent_bridge.py        # Ponte Python de Voz, Wake-Word e Sessão Antigravity (agy)
├── iniciar_agente_m5.bat     # Inicializador rápido da ponte no Windows
├── web/
│   ├── index.html            # Instalador Web Serial (GitHub Pages)
│   └── manifest.json         # Manifesto de binários do ESP Web Tools
├── .github/workflows/
│   └── firmware.yml          # CI/CD automatizado: compilação e deploy do Web Flash
└── README.md
```

---

## 📄 Licença

Distribuído sob licença aberta para a comunidade Maker e entusiastas de IoT.  
Desenvolvido por **João Vitor Ribeiro**.
