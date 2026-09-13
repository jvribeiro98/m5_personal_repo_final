# 🎮 M5 Personal — Universal Multi-IR, Wi-Fi Remote & Air Mouse BLE (M5StickC Plus2)

[![PlatformIO](https://img.shields.io/badge/PlatformIO-Compatible-FF8C00?style=for-the-badge&logo=platformio&logoColor=white)](https://platformio.org/)
[![Arduino ESP32](https://img.shields.io/badge/Arduino_ESP32-3.3.8-00979D?style=for-the-badge&logo=arduino&logoColor=white)](https://espressif.github.io/arduino-esp32/)
[![M5Unified](https://img.shields.io/badge/M5Unified-0.2.19+-E60012?style=for-the-badge)](https://github.com/m5stack/M5Unified)
[![NimBLE](https://img.shields.io/badge/NimBLE_Arduino-2.2+-0080FF?style=for-the-badge)](https://github.com/h2zero/NimBLE-Arduino)
[![IRremoteESP8266](https://img.shields.io/badge/IRremoteESP8266-v2.9+-107C41?style=for-the-badge)](https://github.com/crankyoldgit/IRremoteESP8266)
[![Web Flash](https://img.shields.io/badge/Web_Flash-ESP_Web_Tools-4285F4?style=for-the-badge&logo=googlechrome&logoColor=white)](https://jvribeiro98.github.io/m5_personal_repo_final/)

> **Firmware multifuncional em C++ para M5StickC Plus2 (ESP32-PICO-V3-02) integrando Air Mouse BLE por giroscópio (sem deriva), controle remoto infravermelho universal para Ar-Condicionado e TVs, interface Web responsiva, gerenciador Wi-Fi (NVS), relógio RTC com previsão do tempo e gravação direta pelo navegador via Web Serial.**

---

## ✨ Principais Funcionalidades

- 🖱️ **Air Mouse BLE (Bluetooth Low Energy HID):**
  - Controle do cursor do mouse no PC ou Celular pelo movimento do punho no ar.
  - **Zero-Drift:** Calibração automática de repouso em alta precisão ao entrar na tela e auto-zero contínuo.
  - **Click Stabilization:** Amortecimento automático do cursor durante o clique mecânico para evitar desvios acidentais do alvo.
  - **Orientação Portrait (135x240):** Uso ergonômico natural em pé, com o botão grande sob o polegar e mira gráfica em tempo real.
  - Compatível nativamente com Windows, Android, macOS, Linux, iOS e iPadOS sem nenhum aplicativo ou driver extra.
- ❄️ **Controle de Ar-Condicionado (IR):**
  - Suporte completo a Samsung (protocolos 114/168-bit), Midea (48-bit) e modelos Midea Antigo (COOLIX).
  - Ajuste de temperatura, modo (Frio/Auto), ventilação, Turbo e Swing.
- 📺 **Controle de Televisores (IR):**
  - TV Samsung (NEC 32-bit), TV LG (32-bit) e TV TCL com teclado de navegação e atalhos.
- 📶 **Gerenciador Wi-Fi & WebUI Local:**
  - Armazenamento persistente (NVS) de até 10 redes com prioridade por sinal (RSSI).
  - WebServer local com interface responsiva para controlar IR e Wi-Fi direto pelo smartphone ou computador.
- ⏰ **Relógio RTC & Previsão do Tempo:**
  - Sincronização automática de horário via NTP e clima em tempo real via Open-Meteo.
- 🐎 **Módulo Team (Treinamento & Contador):**
  - Contador de gado e gerenciamento de passadas de cavalos com histórico de treinos.

---

## 🏗️ Arquitetura do Sistema

```mermaid
graph TD
    subgraph "Hardware (M5StickC Plus2)"
        MCU["ESP32-PICO-V3-02 (240MHz, 8MB Flash)"]
        IMU["Sensor IMU 6-Eixos (MPU6886 / BMI270)"]
        IR_LED["Emissor IR Interno (GPIO 19)"]
        TFT["Display LCD TFT 1.14\" (ST7789v2)"]
        BTNS["Botões Físicos (A frontal, B lateral, C power)"]
        PWR["PMIC / Bateria / RTC"]
    end

    subgraph "Módulos de Controle"
        BLE_MOUSE["🖱️ Air Mouse BLE (NimBLE HID Driver)"]
        IR_AC["❄️ Ar-Condicionado (Samsung, Midea, Coolix)"]
        IR_TV["📺 Televisores (Samsung, LG, TCL)"]
        TEAM["🐎 Contador de Passadas & Treino"]
    end

    subgraph "Conectividade & Sistema"
        WIFI_MGR["📶 Gerenciador Wi-Fi (Auto-scan + NVS)"]
        WEB_SRV["🌐 WebServer & Portal Local"]
        CLOCK["⏰ RTC Interno + NTP & Clima"]
    end

    IMU --> BLE_MOUSE
    BTNS --> MCU
    MCU --> TFT
    MCU --> PWR

    BLE_MOUSE --> MCU
    IR_AC --> IR_LED
    IR_TV --> IR_LED
    MCU --> WIFI_MGR
    MCU --> WEB_SRV
    MCU --> CLOCK
    MCU --> TEAM
```

---

## 🕹️ Mapeamento de Botões Físicos

### Modo Geral (Menus do Sistema)
| Botão | Toque Rápido | Segurar (> 1.2s) |
| :--- | :--- | :--- |
| **Botão A (Grande Frontal)** | Selecionar / Confirmar / Executar | — |
| **Botão B (Lateral)** | Próximo item do menu / Incrementar | Voltar ao menu anterior |
| **Botão C / PWR (Topo)** | Item anterior do menu / Decrementar | Desligar (*Power Off*) |

### Modo Air Mouse BLE
| Botão | Ação | Função no Cursor |
| :--- | :--- | :--- |
| **Botão A (Grande Frontal)** | 1 Clique | **Clique Esquerdo** (Bolinha pisca verde) |
| **Botão A (Grande Frontal)** | Pressionar e Segurar | **Arrastar / Segurar botão esquerdo** |
| **Botão B (Lateral)** | 1 Clique ou Segurar | **Clique Direito** (Bolinha pisca ciano) |
| **Botão C / PWR (Topo)** | 1 Clique | Voltar ao Menu Principal |
| **Botão C / PWR (Topo)** | Segurar (> 2s) | **Recalibrar Repouso** (Zera qualquer desvio) |

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
- **Bibliotecas Necessárias:**
  - `M5Unified` (`>= 0.2.19`)
  - `M5GFX` (`>= 0.2.26`)
  - `IRremoteESP8266` (`>= 2.9.0`)
  - `NimBLE-Arduino` (`>= 2.2.3`)

### Compilação via Arduino CLI
```bash
arduino-cli compile --fqbn esp32:esp32:m5stack_stickc_plus2 \
  --build-path build firmware
```

### Gravação via esptool
```bash
esptool.py --chip esp32 --port COM3 --baud 460800 write-flash 0x10000 build/m5_personal.ino.bin
```

---

## ⚙️ Estrutura do Repositório

```text
├── firmware/
│   ├── m5_personal.ino       # Firmware principal em C++
│   ├── M5StickBleMouse.h     # Driver NimBLE BLE HID Mouse otimizado
│   └── MouseCalibration.h   # Algoritmo de calibração de repouso e zero-drift
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
