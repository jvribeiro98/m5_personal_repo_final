# CHECK-POINT 1 — M5 Personal v0.8

Este documento fixa o comportamento e os padrões definidos até esta fase. Novos módulos devem respeitar estas regras, salvo mudança expressamente aprovada.

## 1. Hardware e navegação física

Hardware-alvo: M5StickC Plus 2, tela em rotação 3 e emissor infravermelho interno no GPIO 19.

Controles gerais:
- A curto: selecionar ou executar.
- B curto: próximo item.
- B longo: voltar.
- C curto: item anterior.
- C longo: desligar o M5.
- Dentro do teclado, B/C longos passam a controlar movimento vertical e não desligam o aparelho.

Todos os menus precisam:
- mostrar integralmente a opção selecionada;
- fazer rolagem horizontal suave quando o texto não couber;
- usar paginação/rolagem vertical quando houver mais itens que a tela comporta;
- mostrar indicador vertical de posição quando necessário.

## 2. Menu principal

Módulos atuais:
- Infravermelho.
- Wi-Fi.

A arquitetura visual e a Web UI mantêm espaço para novos módulos.

## 3. Módulo infravermelho

Dispositivos atuais:
- TVs: Samsung e LG funcionais; TCL permanece pendente.
- Ar-condicionado: Samsung e Midea.

Fluxo local:
Menu principal → Infravermelho → TV ou ar-condicionado → marca → controle.

Fluxo Web UI:
Início → Infravermelho → TVs ou Ar-condicionado → marca → interface específica.

Regras:
- A mesma função interna de IR deve ser chamada pela tela física e pela Web UI.
- A Web UI não abre pop-up a cada comando.
- Um ponto luminoso vermelho pisca para confirmar que o comando foi enviado.
- A página de TV segue formato de controle remoto com direcional.
- A página de ar mostra temperatura e botões próprios de climatização.
- A página inicial do IR exibe a recomendação de deixar o M5 perto do aparelho.

## 4. Módulo Wi-Fi

Menu:
- Conectar.
- Conectar Web UI.
- Web UI Rede.
- Redes salvas.

### Conectar pelo M5
Fluxo:
scan → selecionar SSID → reutilizar senha salva ou abrir teclado → animação → resultado.

Teclado:
- layout QWERTY baseado no fluxo do Bruce;
- A seleciona;
- B/C curtos movem horizontalmente;
- B/C longos movem verticalmente;
- ações: confirmar, maiúsculas/símbolos, apagar, espaço e sair.

### Conectar Web UI
- Abre AP temporário `M5-PERSONAL-SETUP`.
- Senha: `12345678`.
- Página local: `192.168.4.1`.
- A página escaneia redes e permite informar a senha.
- Após conexão bem-sucedida, a Web UI é exposta automaticamente na rede local.

### Web UI Rede
- Ativa ou desativa o servidor na rede conectada.
- O estado exibido deve refletir o servidor real.
- Se estiver habilitada e a conexão cair, volta automaticamente depois da reconexão.

## 5. Redes salvas

Limite: 10 redes.

Cada registro guarda:
- SSID;
- senha;
- estado: não testada, verificada ou atenção;
- motivo conhecido da última falha;
- última intensidade de sinal observada.

Operações:
- conectar;
- adicionar;
- editar SSID;
- editar senha;
- excluir.

Na Web UI há a opção:
`Testar novo SSID/senha antes de salvar`.

Marcada:
- tenta conectar primeiro;
- só confirma a inclusão/edição após sucesso;
- uma edição que falhar preserva os dados anteriores.

Desmarcada:
- salva imediatamente;
- fica como não testada até uma conexão bem-sucedida.

## 6. Conexão automática

Ao ligar:
- escaneia redes;
- compara com as redes salvas;
- tenta primeiro a rede conhecida com melhor sinal;
- se falhar, continua buscando redes conhecidas;
- não trava o restante do firmware.

Após queda:
- repete scans em intervalo controlado;
- reconecta automaticamente;
- restaura a Web UI quando habilitada.

Selecionar uma rede já salva:
- reutiliza a senha;
- não abre o teclado novamente.

## 7. Estado e diagnóstico das redes

Indicadores:
- verificada: conexão bem-sucedida;
- atenção: a rede estava disponível, houve tentativa real e ocorreu falha;
- sem indicador: rede ainda não testada.

Uma rede simplesmente ausente ou fora de alcance:
- não é erro;
- não recebe triângulo;
- não perde o estado anterior apenas por não aparecer no scan.

O triângulo de atenção só aparece quando houve tentativa em uma rede disponível. Quando o ESP32 fornece motivo confiável, a página mostra um card, como:
- autenticação/senha rejeitada;
- falha ao obter IP;
- tempo esgotado;
- causa desconhecida.

O firmware não inventa diagnóstico.

## 8. Padrão da Web UI

Todas as páginas:
- Voltar no canto superior esquerdo.
- Início no canto superior direito.
- visual simples, moderno, responsivo e leve;
- ícones SVG embutidos, sem dependência externa;
- botões compactos e bem distribuídos;
- sem pop-ups para confirmar comandos comuns;
- notificações discretas apenas para falhas ou ações administrativas.

Página inicial:
- cartões de módulos com ícones.

Página Wi-Fi:
- SSID atual;
- intensidade em dBm;
- ícone colorido: vermelho/fraca, amarelo/média, verde/boa;
- Web UI ativa/desativada;
- gerenciamento de redes salvas.

## 9. Persistência e segurança

- Preferences/NVS é usada para credenciais e configurações.
- Senhas não são mostradas depois de salvas.
- Credenciais testadas só substituem dados válidos após sucesso.
- Excluir uma rede remove SSID, senha e estado.
- Excluir a rede atual força nova busca por outra rede conhecida.

## 10. Regra de desenvolvimento

Nenhuma função deve ser entregue como protótipo silencioso.
Quando um comportamento ou mapeamento não estiver confirmado, ele deve ser perguntado antes de entrar no firmware destinado à gravação.
