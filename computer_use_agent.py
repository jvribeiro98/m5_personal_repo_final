# -*- coding: utf-8 -*-
"""
Agente Autônomo de Computer Use para M5StickC Plus 2
Permite ao usuário operar o computador totalmente à distância via voz:
- Controle de mídia (YouTube busca+play+tela cheia, Play/Pause, Tela Cheia, Volume, Mudo)
- Navegação em abas e janelas (Fechar aba, nova aba, alternar janela, minimizar, F5, voltar)
- Rolagem de páginas (Rolar para cima, rolar para baixo)
- Abertura de aplicativos e sites (Chrome, YouTube, WhatsApp, Netflix, Spotify, ChatGPT)
- Digitação e cliques de mouse na tela
- Inspeção e visão da tela ativa
"""

import os
import sys
import time
import re
import urllib.parse
import webbrowser
import subprocess
import os_controller

CHROME_PATH = r"C:\Program Files\Google\Chrome\Application\chrome.exe"
CODEX_DIR = r"C:\Users\Jvrib\Documents\Codex"
CODE_CMD = r"C:\Users\Jvrib\AppData\Local\Programs\cursor\resources\app\codeBin\code.cmd"

def notify_m5(on_progress, title, body):
    """Envia notificação de progresso intermediária para a tela do M5Stick."""
    if on_progress:
        try:
            on_progress(title, body)
        except Exception:
            pass

def open_url(url):
    """Abre URL no Chrome preferencialmente ou navegador padrão."""
    try:
        if os.path.exists(CHROME_PATH):
            subprocess.Popen([CHROME_PATH, url])
            return True
    except Exception:
        pass
    try:
        webbrowser.open(url)
        return True
    except Exception:
        pass
    try:
        subprocess.Popen(f'start {url}', shell=True)
        return True
    except Exception:
        return False

def handle_computer_use(text, on_progress=None):
    """
    Processa comandos de voz de Computer Use.
    Retorna um dicionário de resultado ou None se o comando não for de Computer Use.
    """
    text_clean = text.strip()
    text_lower = text_clean.lower()

    # ============================================================
    # 1. CONTROLE DE MÍDIA & YOUTUBE
    # ============================================================

    # A1) Apenas abrir YouTube
    if text_lower in ['abrir youtube', 'abra o youtube', 'abre o youtube', 'abrir o youtube', 'youtube', 'entra no youtube']:
        print("[COMPUTER USE]: Abrindo YouTube...")
        open_url("https://www.youtube.com")
        return {
            "type": "RESULT",
            "agent": "YouTube",
            "title": "YOUTUBE",
            "text": text_clean,
            "body": "YouTube aberto no navegador!",
            "auto_resume": True
        }

    # A2) Busca e Reprodução Autônoma no YouTube
    # Ex: "pesquise no youtube por lofi hip hop e dê play", "coloque rock classico no youtube", "toca ac/dc"
    m_yt_play = re.search(
        r'(?:(?:abra|abre|inicie|inicia)\s+(?:o\s+)?youtube\s+(?:e\s+)?)?'
        r'(?:coloque|coloca|tocar|toca|toque|reproduzir|reproduza|ouvir|ouça|buscar|busca|busque|pesquisar|pesquise|pesquisa)\s+'
        r'(?:no\s+youtube\s+)?(?:por\s+|sobre\s+)?(.+?)(?:\s+no\s+youtube)?(?:\s+e\s+(?:dê\s+|da\s+)?play)?$',
        text_lower,
        re.IGNORECASE
    )
    if ('youtube' in text_lower or 'vídeo' in text_lower or 'video' in text_lower or 'música' in text_lower or 'musica' in text_lower) and m_yt_play:
        query = m_yt_play.group(1).strip()
        query = re.sub(r'\b(?:no\s+youtube|e\s+d[êe]\s+play|d[êe]\s+play|e\s+da\s+play|da\s+play)\b', '', query, flags=re.IGNORECASE).strip()
        if len(query) > 1:
            print(f"[COMPUTER USE]: Automação YouTube para '{query}'...")
            notify_m5(on_progress, "YOUTUBE", f"Buscando '{query}'...")

            q_enc = urllib.parse.quote(query)
            yt_url = f"https://www.youtube.com/results?search_query={q_enc}"
            
            open_url(yt_url)

            # Aguarda a página do YouTube carregar
            notify_m5(on_progress, "YOUTUBE", "Carregando resultados...")
            time.sleep(1.8)

            # O primeiro vídeo de resultado no YouTube fica aproximadamente em x=540, y=370 (1080p)
            w, h = os_controller.get_screen_size()
            first_video_x = int(w * 0.28)
            first_video_y = int(h * 0.35)

            notify_m5(on_progress, "YOUTUBE", "Clicando no vídeo...")
            os_controller.mouse_click(first_video_x, first_video_y, button='left')

            # Aguarda o player abrir e aciona tela cheia (tecla 'f')
            time.sleep(1.6)
            notify_m5(on_progress, "YOUTUBE", "Ativando tela cheia...")
            os_controller.key_press('f')

            return {
                "type": "RESULT",
                "agent": "YouTube Agent",
                "title": "EM REPRODUÇÃO",
                "text": text_clean,
                "body": f"'{query}' reproduzindo em tela cheia no YouTube!",
                "auto_resume": True
            }

    # B) Play / Pause / Retomar
    if any(w in text_lower for w in ['pause', 'pausar', 'pausa', 'despausar', 'despause', 'retomar', 'continue', 'continuar', 'dar play', 'dá play']) and not any(w in text_lower for w in ['ar', 'ar-condicionado']):
        print("[COMPUTER USE]: Play/Pause disparado...")
        os_controller.media_play_pause()
        os_controller.key_press('space')
        return {
            "type": "RESULT",
            "agent": "Mídia PC",
            "title": "PLAY / PAUSE",
            "text": text_clean,
            "body": "Reprodução pausada ou retomada no PC!",
            "auto_resume": True
        }

    # C) Tela Cheia / Sair de Tela Cheia
    if any(w in text_lower for w in ['tela cheia', 'tela inteira', 'fullscreen', 'maximizar vídeo', 'maximizar video', 'sair de tela cheia']):
        print("[COMPUTER USE]: Alternando Tela Cheia...")
        os_controller.key_press('f')
        return {
            "type": "RESULT",
            "agent": "Tela Cheia",
            "title": "TELA CHEIA",
            "text": text_clean,
            "body": "Modo de tela cheia alternado!",
            "auto_resume": True
        }

    # D) Próximo Vídeo / Pular
    if any(w in text_lower for w in ['proximo video', 'próximo vídeo', 'proxima musica', 'próxima música', 'pular video', 'pula esse', 'proxima faixa', 'próxima faixa']):
        print("[COMPUTER USE]: Próxima mídia...")
        os_controller.hotkey('shift', 'n')
        return {
            "type": "RESULT",
            "agent": "Mídia PC",
            "title": "PRÓXIMO",
            "text": text_clean,
            "body": "Avançado para o próximo vídeo/faixa!",
            "auto_resume": True
        }

    # ============================================================
    # 2. CONTROLE DE VOLUME DO COMPUTADOR
    # ============================================================
    if any(w in text_lower for w in ['volume do pc', 'som do pc', 'volume do computador', 'som do computador']) or (('volume' in text_lower or 'som' in text_lower) and not any(w in text_lower for w in ['tv', 'televisao', 'televisão'])):
        if any(w in text_lower for w in ['aumenta', 'aumente', 'aumentar', 'sobe', 'suba', 'subir', 'mais alto']):
            print("[COMPUTER USE]: Volume PC UP...")
            os_controller.volume_up(steps=6)
            return {
                "type": "RESULT",
                "agent": "Volume PC",
                "title": "VOLUME PC +",
                "text": text_clean,
                "body": "Volume do computador aumentado!",
                "auto_resume": True
            }
        elif any(w in text_lower for w in ['diminui', 'diminua', 'diminuir', 'baixa', 'baixe', 'baixar', 'mais baixo', 'abaixa', 'abaixe']):
            print("[COMPUTER USE]: Volume PC DOWN...")
            os_controller.volume_down(steps=6)
            return {
                "type": "RESULT",
                "agent": "Volume PC",
                "title": "VOLUME PC -",
                "text": text_clean,
                "body": "Volume do computador reduzido!",
                "auto_resume": True
            }
        elif any(w in text_lower for w in ['mudo', 'muda', 'mute', 'mutar', 'silencio', 'silêncio', 'silencia', 'silencie', 'sem som', 'tira o som']):
            print("[COMPUTER USE]: Volume PC MUTE...")
            os_controller.volume_mute()
            return {
                "type": "RESULT",
                "agent": "Volume PC",
                "title": "PC MUDO",
                "text": text_clean,
                "body": "Áudio do computador mutado!",
                "auto_resume": True
            }

    # ============================================================
    # 3. ROLAGEM DE PÁGINA (SCROLL)
    # ============================================================
    if any(w in text_lower for w in ['role para baixo', 'rola para baixo', 'desce a pagina', 'desce a página', 'rolar para baixo', 'mais para baixo', 'desce mais', 'rola mais']):
        print("[COMPUTER USE]: Rolando para baixo...")
        os_controller.mouse_scroll(-6)
        return {
            "type": "RESULT",
            "agent": "Scroll",
            "title": "PAGINA BAIXO",
            "text": text_clean,
            "body": "Página rolada para baixo!",
            "auto_resume": True
        }

    if any(w in text_lower for w in ['role para cima', 'rola para cima', 'sobe a pagina', 'sobe a página', 'rolar para cima', 'mais para cima', 'sobe mais', 'rola pra cima']):
        print("[COMPUTER USE]: Rolando para cima...")
        os_controller.mouse_scroll(6)
        return {
            "type": "RESULT",
            "agent": "Scroll",
            "title": "PAGINA CIMA",
            "text": text_clean,
            "body": "Página rolada para cima!",
            "auto_resume": True
        }

    # ============================================================
    # 4. GERENCIAMENTO DE ABAS E NAVEGAÇÃO WEB
    # ============================================================

    # Nova Aba
    if any(w in text_lower for w in ['nova aba', 'abrir nova aba', 'abre outra aba', 'mais uma aba']):
        print("[COMPUTER USE]: Nova aba Ctrl+T...")
        os_controller.hotkey('ctrl', 't')
        return {
            "type": "RESULT",
            "agent": "Navegador",
            "title": "NOVA ABA",
            "text": text_clean,
            "body": "Nova aba aberta no navegador!",
            "auto_resume": True
        }

    # Fechar Aba
    if any(w in text_lower for w in ['feche essa aba', 'fecha essa aba', 'fechar aba', 'fecha a aba', 'feche a aba']):
        print("[COMPUTER USE]: Fechando aba Ctrl+W...")
        os_controller.hotkey('ctrl', 'w')
        return {
            "type": "RESULT",
            "agent": "Navegador",
            "title": "ABA FECHADA",
            "text": text_clean,
            "body": "Aba atual fechada!",
            "auto_resume": True
        }

    # Atualizar / Recarregar Página
    if any(w in text_lower for w in ['atualizar pagina', 'atualizar a pagina', 'recarregar pagina', 'recarregar a pagina', 'atualiza a tela', 'f5']):
        print("[COMPUTER USE]: Recarregando página F5...")
        os_controller.key_press('f5')
        return {
            "type": "RESULT",
            "agent": "Navegador",
            "title": "RECARREGADO",
            "text": text_clean,
            "body": "Página recarregada (F5)!",
            "auto_resume": True
        }

    # Voltar Página
    if any(w in text_lower for w in ['voltar pagina', 'volta a pagina', 'pagina anterior', 'volte a pagina']):
        print("[COMPUTER USE]: Voltar página Alt+Left...")
        os_controller.hotkey('alt', 'left')
        return {
            "type": "RESULT",
            "agent": "Navegador",
            "title": "VOLTAR",
            "text": text_clean,
            "body": "Navegação: Página anterior!",
            "auto_resume": True
        }

    # Avançar Página
    if any(w in text_lower for w in ['avancar pagina', 'avançar pagina', 'proxima pagina', 'próxima página']):
        print("[COMPUTER USE]: Avançar página Alt+Right...")
        os_controller.hotkey('alt', 'right')
        return {
            "type": "RESULT",
            "agent": "Navegador",
            "title": "AVANÇAR",
            "text": text_clean,
            "body": "Navegação: Próxima página!",
            "auto_resume": True
        }

    # Fechar Janela / Programa
    if any(w in text_lower for w in ['feche o programa', 'fecha o programa', 'feche a janela', 'fecha essa janela', 'fechar janela', 'feche o chrome', 'fecha o chrome', 'feche o navegador', 'fecha o navegador']):
        print("[COMPUTER USE]: Fechando janela Alt+F4...")
        os_controller.hotkey('alt', 'f4')
        return {
            "type": "RESULT",
            "agent": "Janela",
            "title": "FECHADO",
            "text": text_clean,
            "body": "Janela atual fechada!",
            "auto_resume": True
        }

    # Alternar Janela (Alt+Tab)
    if any(w in text_lower for w in ['trocar janela', 'troca de janela', 'alternar janela', 'proxima janela', 'mudar de janela']):
        print("[COMPUTER USE]: Alternando janela Alt+Tab...")
        os_controller.hotkey('alt', 'tab')
        time.sleep(0.25)
        title = os_controller.get_active_window_title()
        return {
            "type": "RESULT",
            "agent": "Janela",
            "title": "ALTERNADO",
            "text": text_clean,
            "body": f"Janela ativa: '{title[:35]}'",
            "auto_resume": True
        }

    # Mostrar Área de Trabalho (Win+D)
    if any(w in text_lower for w in ['mostrar area de trabalho', 'mostrar área de trabalho', 'minimizar tudo', 'vai para o desktop']):
        print("[COMPUTER USE]: Minimizar tudo Win+D...")
        os_controller.hotkey('win', 'd')
        return {
            "type": "RESULT",
            "agent": "Desktop",
            "title": "DESKTOP",
            "text": text_clean,
            "body": "Área de trabalho exibida!",
            "auto_resume": True
        }

    # ============================================================
    # 5. ABERTURA DE APLICATIVOS E WEBSITES COMUNS
    # ============================================================

    # Google Chrome / Navegador
    if any(w in text_lower for w in ['abrir chrome', 'abra o chrome', 'abre o chrome', 'abrir navegador', 'abra o navegador', 'abre o navegador', 'abrir internet', 'google chrome']):
        print("[COMPUTER USE]: Abrindo Google Chrome...")
        open_url("https://www.google.com")
        return {
            "type": "RESULT",
            "agent": "Chrome",
            "title": "CHROME",
            "text": text_clean,
            "body": "Google Chrome aberto no computador!",
            "auto_resume": True
        }

    # Busca no Google
    m_search = re.search(r'^(?:pesquisar|pesquise|procurar|procure|buscar|busca|pesquisa)\s+(?:no\s+google\s+|na\s+internet\s+|na\s+web\s+)?(?:por\s+|sobre\s+)?(.+)', text_lower)
    if m_search and not any(w in text_lower for w in ['youtube', 'vídeo', 'video']):
        query = m_search.group(1).strip()
        print(f"[COMPUTER USE]: Busca Google: '{query}'...")
        open_url(f"https://www.google.com/search?q={urllib.parse.quote(query)}")
        return {
            "type": "RESULT",
            "agent": "Busca Web",
            "title": "BUSCA GOOGLE",
            "text": text_clean,
            "body": f"Pesquisando '{query}' no Google!",
            "auto_resume": True
        }

    # WhatsApp Web
    if any(w in text_lower for w in ['whatsapp', 'zap', 'zap zap', 'whats']) and any(w in text_lower for w in ['abrir', 'abre', 'abra', 'iniciar']):
        print("[COMPUTER USE]: Abrindo WhatsApp Web...")
        open_url("https://web.whatsapp.com")
        return {
            "type": "RESULT",
            "agent": "WhatsApp",
            "title": "WHATSAPP",
            "text": text_clean,
            "body": "WhatsApp Web aberto no navegador!",
            "auto_resume": True
        }

    # Netflix
    if 'netflix' in text_lower and any(w in text_lower for w in ['abrir', 'abre', 'abra', 'iniciar']):
        print("[COMPUTER USE]: Abrindo Netflix...")
        open_url("https://www.netflix.com")
        return {
            "type": "RESULT",
            "agent": "Netflix",
            "title": "NETFLIX",
            "text": text_clean,
            "body": "Netflix aberto no navegador!",
            "auto_resume": True
        }

    # ChatGPT
    if ('chatgpt' in text_lower or 'chat gpt' in text_lower) and any(w in text_lower for w in ['abrir', 'abre', 'abra']):
        print("[COMPUTER USE]: Abrindo ChatGPT...")
        open_url("https://chatgpt.com")
        return {
            "type": "RESULT",
            "agent": "ChatGPT",
            "title": "CHATGPT",
            "text": text_clean,
            "body": "ChatGPT aberto no navegador!",
            "auto_resume": True
        }

    # Twitch
    if 'twitch' in text_lower and any(w in text_lower for w in ['abrir', 'abre', 'abra']):
        print("[COMPUTER USE]: Abrindo Twitch...")
        open_url("https://www.twitch.tv")
        return {
            "type": "RESULT",
            "agent": "Twitch",
            "title": "TWITCH",
            "text": text_clean,
            "body": "Twitch aberto no navegador!",
            "auto_resume": True
        }

    # ============================================================
    # 6. CLIQUES E DIGITAÇÃO
    # ============================================================

    # Clique no meio da tela
    if any(w in text_lower for w in ['clique no meio', 'clica no meio', 'clique na tela', 'clica na tela']):
        w, h = os_controller.get_screen_size()
        os_controller.mouse_click(w // 2, h // 2)
        return {
            "type": "RESULT",
            "agent": "Mouse",
            "title": "CLIQUE",
            "text": text_clean,
            "body": f"Clique realizado no centro da tela ({w//2}, {h//2})!",
            "auto_resume": True
        }

    # Digitar texto e dar enter
    m_type = re.search(r'^(?:digite|digita|escreva|escreve)\s+(.+?)(?:\s+e\s+(?:dê|da)?\s*enter)?$', text_lower)
    if m_type:
        text_to_type = m_type.group(1).strip()
        should_enter = bool(re.search(r'\b(?:enter|entra)\b', text_lower))
        text_to_type = re.sub(r'\s+e\s+(?:d[êe]|da)?\s*enter$', '', text_to_type, flags=re.IGNORECASE).strip()
        print(f"[COMPUTER USE]: Digitando '{text_to_type}' (Enter={should_enter})...")
        os_controller.type_text(text_to_type)
        if should_enter:
            time.sleep(0.1)
            os_controller.key_press('enter')
        return {
            "type": "RESULT",
            "agent": "Teclado",
            "title": "DIGITADO",
            "text": text_clean,
            "body": f"'{text_to_type}' digitado no computador!",
            "auto_resume": True
        }

    # Pressionar Enter / Esc / Tecla
    if text_lower in ['enter', 'dê enter', 'da enter', 'confirmar']:
        os_controller.key_press('enter')
        return {
            "type": "RESULT",
            "agent": "Teclado",
            "title": "ENTER",
            "text": text_clean,
            "body": "Tecla Enter pressionada!",
            "auto_resume": True
        }

    if text_lower in ['escape', 'esc', 'cancela', 'cancelar']:
        os_controller.key_press('esc')
        return {
            "type": "RESULT",
            "agent": "Teclado",
            "title": "ESC",
            "text": text_clean,
            "body": "Tecla Escape pressionada!",
            "auto_resume": True
        }

    # ============================================================
    # 7. VISÃO DA TELA (INSPEÇÃO VISUAL)
    # ============================================================
    if any(w in text_lower for w in ['o que esta na tela', 'o que está na tela', 'veja a tela', 'olhe a tela', 'leia a tela', 'o que tem na tela']):
        print("[COMPUTER USE]: Capturando tela para inspeção visual...")
        notify_m5(on_progress, "VISÃO ATIVA", "Analisando janela...")
        title = os_controller.get_active_window_title()
        im = os_controller.capture_screen()
        w, h = im.size

        snap_path = os.path.join(CODEX_DIR, "vai", "screen_snapshot.jpg")
        try:
            im.save(snap_path, quality=85)
        except Exception:
            pass

        return {
            "type": "RESULT",
            "agent": "Visão PC",
            "title": "JANELA ATIVA",
            "text": text_clean,
            "body": f"Janela ativa: '{title}'. Resolução: {w}x{h}.",
            "auto_resume": True
        }

    # Não é comando de controle direto do computador
    return None
