# -*- coding: utf-8 -*-
"""Comandos de voz diretos para mídia, navegador e entrada no Windows."""

import os
import re
import subprocess
import time
import unicodedata
import urllib.parse
import webbrowser

import os_controller

CHROME_PATH = os.path.join(os.environ.get("PROGRAMFILES", r"C:\Program Files"),
                           "Google", "Chrome", "Application", "chrome.exe")


def notify_m5(on_progress, title, body):
    """Falhar ao mostrar progresso não interrompe a ação solicitada."""
    if on_progress:
        try:
            on_progress(title, body)
        except Exception:
            pass


def open_url(url):
    """Abre uma URL HTTP(S), sem interpretar seu conteúdo como comando shell."""
    if not isinstance(url, str) or any(ord(char) < 32 for char in url):
        return False
    try:
        parsed = urllib.parse.urlsplit(url)
        if parsed.scheme.lower() not in {"http", "https"} or not parsed.hostname:
            return False
        parsed.port  # Rejeita uma porta malformada antes de abrir o navegador.
    except ValueError:
        return False
    try:
        if os.path.exists(CHROME_PATH):
            subprocess.Popen([CHROME_PATH, url])
            return True
    except OSError:
        pass
    try:
        return bool(webbrowser.open(url))
    except (OSError, webbrowser.Error):
        return False


def _normalize(text):
    text = unicodedata.normalize("NFD", text.casefold())
    return " ".join("".join(c for c in text if not unicodedata.combining(c)).split())


def _result(text, title, body, agent="Computer Use", success=True):
    # RESULT também é o envelope de erro compreendido pelo firmware.
    return {"type": "RESULT", "agent": agent, "title": title, "text": text,
            "body": body, "auto_resume": success}


def _open_result(text, url, title, body):
    if not open_url(url):
        return _result(text, "ERRO", "Não foi possível abrir o navegador.", success=False)
    return _result(text, title, body, "Navegador")


# Cada alias representa um comando inteiro. Texto citado, negado ou destinado a
# pesquisa/digitação não deve acionar atalhos apenas por conter estas palavras.
_DIRECT_ACTIONS = (
    ("tela cheia|tela inteira|fullscreen|maximizar video|sair de tela cheia", "key_press", ("f",), "TELA CHEIA", "Comando de tela cheia enviado."),
    ("proximo video|proxima musica|pular video|pula esse|proxima faixa", "hotkey", ("shift", "n"), "PRÓXIMO", "Comando de próxima mídia enviado."),
    ("role para baixo|rola para baixo|desce a pagina|rolar para baixo|mais para baixo|desce mais|rola mais|rola pra baixo", "mouse_scroll", (-6,), "PAGINA BAIXO", "Página rolada para baixo."),
    ("role para cima|rola para cima|sobe a pagina|rolar para cima|mais para cima|sobe mais|rola pra cima", "mouse_scroll", (6,), "PAGINA CIMA", "Página rolada para cima."),
    ("nova aba|abrir nova aba|abra uma nova aba|abre outra aba|mais uma aba", "hotkey", ("ctrl", "t"), "NOVA ABA", "Comando de nova aba enviado."),
    ("feche essa aba|fecha essa aba|fechar aba|fecha a aba|feche a aba", "hotkey", ("ctrl", "w"), "ABA FECHADA", "Comando para fechar a aba enviado."),
    ("atualizar pagina|atualizar a pagina|recarregar pagina|recarregar a pagina|atualiza a tela|f5", "key_press", ("f5",), "RECARREGADO", "Comando para recarregar enviado."),
    ("voltar pagina|volta a pagina|pagina anterior|volte a pagina", "hotkey", ("alt", "left"), "VOLTAR", "Comando de página anterior enviado."),
    ("avancar pagina|proxima pagina", "hotkey", ("alt", "right"), "AVANÇAR", "Comando de próxima página enviado."),
    ("feche o programa|fecha o programa|feche a janela|fecha essa janela|fechar janela|feche o chrome|fecha o chrome|feche o navegador|fecha o navegador", "hotkey", ("alt", "f4"), "FECHADO", "Comando para fechar a janela enviado."),
    ("mostrar area de trabalho|minimizar tudo|vai para o desktop", "hotkey", ("win", "d"), "DESKTOP", "Comando de área de trabalho enviado."),
    ("enter|de enter|da enter|confirmar", "key_press", ("enter",), "ENTER", "Tecla Enter pressionada."),
    ("escape|esc|cancela|cancelar", "key_press", ("esc",), "ESC", "Tecla Escape pressionada."),
)

_SITES = {
    "youtube": ("https://www.youtube.com", "YOUTUBE"),
    "chrome": ("https://www.google.com", "CHROME"),
    "google chrome": ("https://www.google.com", "CHROME"),
    "navegador": ("https://www.google.com", "NAVEGADOR"),
    "internet": ("https://www.google.com", "NAVEGADOR"),
    "whatsapp": ("https://web.whatsapp.com", "WHATSAPP"),
    "whats": ("https://web.whatsapp.com", "WHATSAPP"),
    "zap": ("https://web.whatsapp.com", "WHATSAPP"),
    "zap zap": ("https://web.whatsapp.com", "WHATSAPP"),
    "netflix": ("https://www.netflix.com", "NETFLIX"),
    "chatgpt": ("https://chatgpt.com", "CHATGPT"),
    "chat gpt": ("https://chatgpt.com", "CHATGPT"),
    "twitch": ("https://www.twitch.tv", "TWITCH"),
}


def handle_computer_use(text, on_progress=None):
    """Retorna RESULT para um comando reconhecido ou None para outra intenção."""
    if not isinstance(text, str) or not text.strip():
        return None
    text_clean = text.strip()
    try:
        return _handle_command(text_clean, on_progress)
    except (OSError, RuntimeError, ValueError, TypeError) as exc:
        return _result(text_clean, "ERRO", f"Falha no controle do computador: {exc}", success=False)


def _handle_command(text, on_progress):
    # Preserve caixa e acentos do payload. Só o sufixo explícito solicita Enter.
    typed = re.fullmatch(r"(?:digite|digita|escreva|escreve)\s+(.+)", text,
                         flags=re.IGNORECASE | re.DOTALL)
    if typed:
        payload = typed.group(1)
        enter = re.search(r"\s+e\s+(?:(?:d[êe]|da)\s+)?enter\s*[.!]?\s*$", payload, re.IGNORECASE)
        if enter:
            payload = payload[:enter.start()]
        os_controller.type_text(payload)
        if enter:
            os_controller.key_press("enter")
        return _result(text, "DIGITADO", "Texto digitado no computador.", "Teclado")

    command = _normalize(text).rstrip(".!?")
    command = re.sub(r"^por favor[, ]+", "", command).strip()
    command = re.sub(r"[, ]+por favor$", "", command).strip()

    # A pesquisa também tem payload; resolva-a antes das ações de mídia/janelas.
    search = re.fullmatch(
        r"(?:(?:abra|abre|inicie|inicia)\s+(?:o\s+)?youtube\s+e\s+)?"
        r"(?:pesquisar|pesquise|pesquisa|procurar|procure|buscar|busca|busque|"
        r"coloque|coloca|tocar|toca|toque|reproduzir|reproduza|ouvir|ouça)\s+"
        r"(?:(no\s+(?:google|youtube)|na\s+(?:internet|web))\s+)?"
        r"(?:por\s+|sobre\s+)?(.+)", text, re.IGNORECASE)
    if search:
        location = _normalize(search.group(1) or "")
        query = search.group(2).strip()
        is_youtube = location == "no youtube" or (not location and bool(
            re.search(r"\b(?:youtube|video|musica)\b", _normalize(text))))
        if is_youtube:
            query = re.sub(r"\s+e\s+(?:(?:d[êe]|da)\s+)?play[.!]?$", "", query, flags=re.IGNORECASE)
            query = re.sub(r"\s+no\s+youtube[.!]?$", "", query, flags=re.IGNORECASE).strip()
            notify_m5(on_progress, "YOUTUBE", f"Buscando '{query}'...")
            return _open_result(text, "https://www.youtube.com/results?" +
                                urllib.parse.urlencode({"search_query": query}),
                                "BUSCA YOUTUBE", f"Busca por '{query}' aberta. Selecione o vídeo para reproduzir.")
        if re.match(r"(?:pesquis|procur|busc)", _normalize(text)):
            return _open_result(text, "https://www.google.com/search?" +
                                urllib.parse.urlencode({"q": query}),
                                "BUSCA GOOGLE", f"Pesquisando '{query}' no Google.")

    if re.fullmatch(r"(?:pause|pausar|pausa|despausar|despause|retomar|continue|continuar|dar play|da play)"
                    r"(?:\s+(?:(?:o|a)\s+)?(?:video|musica|reproducao))?", command):
        os_controller.media_play_pause()
        return _result(text, "PLAY / PAUSE", "Comando de pausa/retomada enviado ao PC.", "Mídia PC")

    # TV/ar-condicionado ficam para o despachante de infravermelho da ponte.
    volume = re.fullmatch(
        r"(aumenta|aumente|aumentar|sobe|suba|subir|diminui|diminua|diminuir|baixa|baixe|baixar|abaixa|abaixe|"
        r"mudo|muda|mute|mutar|silencio|silencia|silencie|tira)\s+(?:o\s+)?"
        r"(?:volume|som|audio)(?:\s+do\s+(?:pc|computador))?", command)
    if volume:
        verb = volume.group(1)
        if verb in {"aumenta", "aumente", "aumentar", "sobe", "suba", "subir"}:
            os_controller.volume_up(steps=6)
            return _result(text, "VOLUME PC +", "Volume do computador aumentado.")
        if verb in {"diminui", "diminua", "diminuir", "baixa", "baixe", "baixar", "abaixa", "abaixe"}:
            os_controller.volume_down(steps=6)
            return _result(text, "VOLUME PC -", "Volume do computador reduzido.")
        os_controller.volume_mute()
        return _result(text, "PC MUDO", "Comando de mudo alternado no computador.")
    if re.fullmatch(r"(?:(?:mute|mutar|silencie)\s+(?:o\s+)?(?:pc|computador)|sem som)", command):
        os_controller.volume_mute()
        return _result(text, "PC MUDO", "Comando de mudo alternado no computador.")

    for aliases, method, args, title, body in _DIRECT_ACTIONS:
        if command in aliases.split("|"):
            getattr(os_controller, method)(*args)
            return _result(text, title, body)

    if command in {"trocar janela", "troca de janela", "alternar janela", "proxima janela", "mudar de janela"}:
        os_controller.hotkey("alt", "tab")
        time.sleep(0.25)
        title = os_controller.get_active_window_title()
        return _result(text, "ALTERNADO", f"Janela ativa: '{title[:80]}'.", "Janela")

    site_match = re.fullmatch(r"(?:abrir|abre|abra|iniciar|inicia|inicie|entra|entrar)\s+(?:(?:o|a|no|na)\s+)?(.+)", command)
    site = site_match.group(1) if site_match else command if command in {"youtube", "google chrome"} else None
    if site in _SITES:
        url, title = _SITES[site]
        return _open_result(text, url, title, f"{title} aberto no navegador.")

    if command in {"clique no meio", "clica no meio", "clique na tela", "clica na tela"}:
        width, height = os_controller.get_screen_size()
        os_controller.mouse_click(width // 2, height // 2)
        return _result(text, "CLIQUE", "Clique realizado no centro da tela.", "Mouse")

    if command in {"o que esta na tela", "veja a tela", "olhe a tela", "leia a tela", "o que tem na tela"}:
        notify_m5(on_progress, "JANELA ATIVA", "Consultando janela...")
        title = os_controller.get_active_window_title()
        width, height = os_controller.get_screen_size()
        return _result(text, "JANELA ATIVA", f"Janela ativa: '{title}'. Resolução: {width}x{height}.", "Visão PC")

    return None
