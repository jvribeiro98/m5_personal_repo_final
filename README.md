# M5 Personal

Firmware pessoal para **M5StickC Plus 2**, organizado em módulos e com interface local no display e Web UI pela rede.

## Estado atual

Versão base: **v0.8.1 — Check-point 1**.

Já implementado:

- menu principal modular;
- controles infravermelhos para TVs Samsung e LG;
- controles de ar-condicionado Samsung e Midea;
- gerenciamento de Wi‑Fi no próprio M5;
- configuração por ponto de acesso temporário;
- Web UI na rede local;
- até 10 redes salvas com estado e diagnóstico;
- reconexão automática sem bloquear o firmware;
- persistência de configurações em Preferences/NVS;
- compilação automática no GitHub;
- gravação pelo navegador com ESP Web Tools.

O comportamento congelado desta versão está documentado em [CHECKPOINT_1.md](CHECKPOINT_1.md).

## Hardware

- M5StickC Plus 2
- ESP32-PICO-V3-02
- Tela em rotação 3
- Emissor infravermelho interno no GPIO 19

## Estrutura

```text
firmware/m5_personal.ino   firmware principal
web/                      página de instalação pelo navegador
.github/workflows/         build e publicação automática
CHECKPOINT_1.md            regras funcionais do Check-point 1
```

## Dependências

- ESP32 Arduino Core 3.3.8
- M5Unified
- IRremoteESP8266
- WiFi, WebServer e Preferences do core ESP32

## Compilação automática

Todo push na branch `main` executa o workflow:

1. instala o Arduino CLI;
2. instala o core ESP32 e as bibliotecas;
3. compila `firmware/m5_personal.ino`;
4. gera um `firmware.bin` consolidado;
5. publica a página de instalação no GitHub Pages.

## Gravar sem Arduino IDE

Abra no Chrome ou Edge:

```text
https://jvribeiro98.github.io/m5stickCplus2_ar_condicionado/
```

Depois:

1. conecte o M5StickC Plus 2 pelo USB;
2. clique em **Conectar e instalar**;
3. selecione a porta serial;
4. confirme a gravação.

## Desenvolvimento

Fluxo esperado:

```text
alterar firmware → commit/push → GitHub compila → abrir página → instalar
```

Nenhuma função deve entrar como protótipo silencioso. Comportamentos e mapeamentos não confirmados precisam ser definidos antes de integrar uma versão destinada à gravação.

## Próximos passos

- validar o controle TCL;
- ampliar os módulos sem quebrar as regras do Check-point 1;
- versionar novos checkpoints funcionais;
- melhorar diagnóstico e atualização do firmware mantendo o fluxo simples.
