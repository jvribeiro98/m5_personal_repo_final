# M5 Personal

Firmware pessoal para **M5StickC Plus 2**, com interface local no display, controles infravermelhos e configuração pela rede.

## Estado atual

Versão base: **v0.8.1 — Check-point 1**.

Já implementado:

- menu principal modular;
- controles infravermelhos para TVs Samsung e LG;
- controles de ar-condicionado Samsung e Midea;
- gerenciamento de Wi-Fi no próprio M5;
- configuração por ponto de acesso temporário;
- Web UI na rede local;
- até 10 redes salvas com estado e diagnóstico;
- reconexão automática sem bloquear o firmware;
- persistência em Preferences/NVS;
- compilação automática no GitHub Actions;
- geração de um `firmware.bin` consolidado;
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
.github/workflows/         compilação e publicação automática
CHECKPOINT_1.md            regras funcionais do Check-point 1
```

## Dependências

- ESP32 Arduino Core 3.3.8
- M5Unified
- IRremoteESP8266
- WiFi, WebServer e Preferences incluídos no core ESP32

## Fluxo automático

Todo push na branch `main` que altere o firmware, a página ou o workflow executa:

1. instalação do Arduino CLI;
2. instalação do core ESP32 e bibliotecas;
3. compilação de `firmware/m5_personal.ino`;
4. consolidação do bootloader, partições e aplicação em `firmware.bin`;
5. atualização automática da versão pelo hash do commit;
6. armazenamento do binário como artefato do GitHub Actions;
7. publicação da página no GitHub Pages.

## Flash pelo navegador

Página do instalador:

```text
https://jvribeiro98.github.io/m5_personal_repo_final/
```

Uso:

1. abra a página no Chrome ou Edge em um computador;
2. conecte o M5StickC Plus 2 por um cabo USB com dados;
3. clique em **Conectar e instalar**;
4. selecione a porta serial do M5;
5. confirme a instalação.

O manifesto e o binário publicados sempre pertencem ao último build bem-sucedido da branch `main`.

## Desenvolvimento

```text
alterar firmware → commit/push → GitHub compila → abrir página → instalar
```

O workflow também disponibiliza o `firmware.bin` em **Actions → execução do build → Artifacts** por 30 dias.

## Próximos passos

- validar o controle TCL;
- ampliar os módulos sem quebrar o Check-point 1;
- versionar novos checkpoints funcionais;
- adicionar atualização OTA quando o fluxo atual estiver validado;
- melhorar diagnóstico e recuperação sem aumentar a complexidade de uso.
