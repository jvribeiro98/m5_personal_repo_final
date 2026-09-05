# 🎮 M5 Personal — Universal Multi-IR & Wi-Fi Remote Firmware (M5StickC Plus2)

[![PlatformIO](https://img.shields.io/badge/PlatformIO-Compatible-FF8C00?style=for-the-badge&logo=platformio&logoColor=white)](https://platformio.org/)
[![Arduino ESP32](https://img.shields.io/badge/Arduino_ESP32-3.3.8-00979D?style=for-the-badge&logo=arduino&logoColor=white)](https://espressif.github.io/arduino-esp32/)
[![M5Unified](https://img.shields.io/badge/M5Unified-0.1.12+-E60012?style=for-the-badge)](https://github.com/m5stack/M5Unified)
[![IRremoteESP8266](https://img.shields.io/badge/IRremoteESP8266-v2.8+-107C41?style=for-the-badge)](https://github.com/crankyoldgit/IRremoteESP8266)
[![Web Flash](https://img.shields.io/badge/Web_Flash-ESP_Web_Tools-4285F4?style=for-the-badge&logo=googlechrome&logoColor=white)](https://jvribeiro98.github.io/m5_personal_repo_final/)

> **Firmware multifuncional em C++ para M5StickC Plus2 (ESP32-PICO-V3-02) com emissor infravermelho interno (GPIO 19), interface visual no display TFT, servidor web integrado, gerenciador de até 10 redes Wi-Fi e gravação direta pelo navegador via Web Serial.**

---

## 🏗️ Arquitetura do Firmware & Módulos

```mermaid
graph TD
    subgraph "Hardware (M5StickC Plus2)"
        MCU["ESP32-PICO-V3-02 Core"]
        IR_LED["Emissor IR Interno (GPIO 19)"]
        TFT["Display LCD TFT 1.14\" (ST7789v2)"]
        BTNS["Botões Físicos (A, B e C)"]
        BAT["PMIC & Monitor de Bateria (AXP192 / M5Unified)"]
    end

    subgraph "Módulos de Controle IR"
        AC_SAMSUNG["❄️ AC Samsung (Protocolo 114/168-bit)"]
        AC_MIDEA["❄️ AC Midea & Coolix (48-bit)"]
        TV_SAMSUNG["📺 TV Samsung (NEC 32-bit)"]
        TV_LG["📺 TV LG (32-bit)"]
    end

    subgraph "Conectividade & Gerenciamento"
        WIFI_MGR["📶 Gerenciador Wi-Fi (Até 10 SSIDs + NVS)"]
        WEB_SRV["🌐 WebServer Local & Portal Captivo"]
        PREFS["💾 Persistência NVS / Preferences"]
    end

    BTNS --> MCU
    MCU --> TFT
    MCU --> BAT
    MCU --> IR_LED
    MCU --> WIFI_MGR
    MCU --> WEB_SRV
    MCU --> PREFS

    MCU --> AC_SAMSUNG
    MCU --> AC_MIDEA
    MCU --> TV_SAMSUNG
    MCU --> TV_LG

    AC_SAMSUNG --> IR_LED
    AC_MIDEA --> IR_LED
    TV_SAMSUNG --> IR_LED
    TV_LG --> IR_LED
```

---

## 🕹️ Mapeamento de Botões Físicos

| Botão | Ação Curta | Ação Longa (Hold > 1.2s) |
| :--- | :--- | :--- |
| **Botão A (Frontal M5)** | Selecionar / Executar / Disparar Comando IR | — |
| **Botão B (Lateral)** | Próximo item do menu / Incrementar | Retornar ao menu anterior |
| **Botão C / Power (Topo)** | Item anterior do menu / Decrementar | Desligar o dispositivo (*Power Off*) |

---

## 📡 Gravação Direta pelo Navegador (Web Flash)

Você pode instalar o firmware diretamente no seu M5StickC Plus2 sem instalar Arduino IDE ou PlatformIO:

1. Acesse o instalador online: **[jvribeiro98.github.io/m5_personal_repo_final](https://jvribeiro98.github.io/m5_personal_repo_final/)**
2. Conecte o M5StickC Plus2 na porta USB do computador (usando Chrome ou Edge).
3. Clique em **"Conectar e Instalar"** e selecione a porta COM correspondente.
4. O processo grava o binário consolidado (`firmware.bin`) compilado automaticamente pelo GitHub Actions.

---

## 🚀 Compilação & Gravação Manual

### Configuração no Arduino IDE
- **Placa**: `M5StickC-Plus2` ou `ESP32-PICO-DevKit`
- **Flash Size**: `8MB (64Mb)`
- **Partition Scheme**: `Default 4MB with spiffs` / `8MB with spiffs`
- **PSRAM**: `Enabled`
- **Bibliotecas Necessárias**:
  - `M5Unified` (`>= 0.1.12`)
  - `IRremoteESP8266` (`>= 2.8.6`)

---

## ⚙️ Estrutura do Repositório

```text
├── firmware/
│   └── m5_personal.ino       # Código-fonte do firmware em C++
├── web/
│   ├── index.html            # Interface do ESP Web Tools (GitHub Pages)
│   └── manifest.json         # Manifesto de binários para Web Flash
├── .github/workflows/
│   └── build.yml             # CI/CD automatizado: compilação Arduino CLI e deploy
└── CHECKPOINT_1.md           # Especificação canônica do Checkpoint 1
```

---

## 📄 Licença

Distribuído sob licença aberta para comunidade Maker e entusiastas de IoT. Desenvolvido por **João Vitor Ribeiro**.
