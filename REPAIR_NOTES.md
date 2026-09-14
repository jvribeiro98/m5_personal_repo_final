# Correções de estabilidade — candidato para teste físico

Base: `cdd6b17cb709b6a6405f5411dd95e7c856575b57`.
Branch: `fix/firmware-stability`.

## O que mudou

- Busca manual e automática de Wi-Fi assíncrona. A reconexão ordena redes visíveis pelo sinal, tenta as conhecidas e descarta a lista após conectar.
- Uma tentativa de conexão não pode ser sobrescrita por outra. Edição e exclusão ficam bloqueadas durante transações e durante o teclado.
- SSID, senha e índices recebem validação. Texto inválido não vira índice zero; renomear para outra rede existente é rejeitado.
- O teclado continua processando conexão e manutenção. A seleção física é invalidada quando a exclusão de uma rede muda os índices.
- A API informa se está conectando. O navegador encerra o acompanhamento após falha e orienta consultar o IP quando perde contato.
- O AP permanece por dez segundos após sucesso para o navegador conseguir ler o novo IP.
- O controle web do ar consulta o estado do firmware ao abrir, depois dos comandos e periodicamente. Não usa temperatura local fictícia.
- Comandos web não mudam o aparelho selecionado fisicamente. IR usa POST e o navegador bloqueia novos cliques enquanto o pedido anterior está em andamento.
- Samsung liga/desliga usando uma chamada `sendExtended()`, com energia e ajustes no mesmo pacote. A sequência `sendOn()` seguida de `send()` foi removida.
- Coolix respeita 17 °C ao diminuir temperatura. A opção de exclusão de rede cabe acima do rodapé.
- JSON escapa caracteres de controle de SSIDs.
- A automação executa testes de comportamento, fixa as versões principais das bibliotecas e não publica pull requests. Foi removida a exigência de um comentário inexistente que impediria o build atual.

## Validação automatizada

`python tests/run_host.py`: 19 cenários executam funções extraídas do firmware, sem reescrever sua lógica. Wi-Fi, tela, emissor IR e armazenamento são substituídos na fronteira de hardware. Esses testes não simulam a pilha de rádio nem o tempo de escrita da flash.

`node --test --test-isolation=none tests/web.test.cjs`: quatro cenários executam o JavaScript extraído da interface web com respostas controladas.

Compilação local com Arduino CLI 1.5.1, ESP32 3.3.8, M5Unified 0.2.19, M5GFX 0.2.26 e IRremoteESP8266 2.9.0. Mantido o alvo de compilação existente `esp32:esp32:esp32`; a partição atual está próxima do limite. Alterar a tabela de partições exige migração e validação própria.

## Teste físico obrigatório antes de chamar de versão estável

1. Conectar a uma rede válida pelo M5. Durante scan e tentativa, verificar resposta dos botões. Após sucesso, abrir o controle IR e enviar um comando.
2. Repetir com senha errada. Confirmar retorno de falha e preservação da rede anterior.
3. Configurar pelo AP, observar o IP exibido, entrar na rede local e abrir a Web UI. Repetir com falha de autenticação.
4. Desligar e religar o roteador. Confirmar reconexão e retorno do servidor, com o controle físico responsivo.
5. Abrir Samsung fisicamente e controlar LG/Midea pelo celular. Confirmar que o alvo físico continua o mesmo.
6. No Samsung, testar ligar e desligar separadamente, verificando energia, temperatura e modo. A alteração elimina o envio duplo explícito, mas a compatibilidade com o modelo real precisa ser confirmada.
7. Testar Midea e Coolix separadamente. Repetições de quadros exigidas por protocolos foram preservadas; contar ações do aparelho, não pulsos do emissor.
8. Reabrir a página do ar e comparar os valores com o M5. O estado é o último solicitado pelo controle: não há confirmação enviada pelo ar ao M5.

## Pendências conhecidas

- O travamento relatado ao conectar ainda precisa ser reproduzido no dispositivo. Foram removidos bloqueios confirmados, mas isso não comprova a eliminação de reset, falha elétrica ou problema na pilha Wi-Fi.
- `saveSavedNetworks()` ainda limpa e regrava o namespace. Uma interrupção de energia no meio pode perder registros; substituir por persistência transacional com migração é trabalho separado e deve ser testado com falhas de escrita.
- Swing/turbo/sleep em protocolos de alternância podem divergir do aparelho quando se usa outro controle ou quando o sinal não chega. Não tratar os indicadores como telemetria confirmada.
- TCL continua sem códigos. Não foram inventados mapeamentos.
- A Web UI continua sem autenticação na rede local; o AP conserva a senha definida no checkpoint.
- Diagnóstico específico de autenticação/DHCP ainda exige coleta de eventos confiáveis. O código não deve inferir senha errada a partir de uma falha genérica.
- O binário ainda mostra a identificação original v0.9.1. Identificar este candidato pelo hash SHA-256 do arquivo entregue, não pelo rótulo antigo da tela.

## Ideias para a próxima etapa

- Favoritos por cômodo e atalhos para os controles usados diariamente.
- Perfis de ar, como dormir/trabalhar, com configurações explícitas e indicação do último comando enviado.
- Página de diagnóstico com uptime, motivo do último reset, memória livre, Wi-Fi e histórico curto de comandos sem senhas.
- Brilho ajustável e suspensão da tela sem interromper o servidor.
- Backup e restauração de configurações com versão e validação.
- Integração com automação residencial após definir autenticação e descoberta.
- Aprendizado de comandos IR como expansão de hardware: depende de receptor compatível; não é uma capacidade implementada pelo emissor atual.

Os recursos acima são propostas, não funcionalidades incluídas nesta correção.
