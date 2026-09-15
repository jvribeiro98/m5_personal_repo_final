# -*- coding: utf-8 -*-
"""
M5StickC Plus 2 - Ponte de Voz & Antigravity (agy) v7.0
- Modo Alexa Mãos-Livres (Wake-Word contínuo "Ei M5" / "M5" / "Alexa")
- Modo Push-to-Talk (PTT)
- Despachante Nativo de Ações do Windows (Chrome, YouTube, VSCode, Terminal, etc.)
- Acionamento direto de Hardware IR (GPIO 19) no M5Stick via JSON ("ir": "...")
- Integração Inteligente com Antigravity CLI (agy) para consultas e automações
"""

import os
import sys
import time
import re
import json
import socket
import subprocess
import threading
import http.server
import urllib.parse
import webbrowser
import numpy as np

try:
    sys.stdout.reconfigure(line_buffering=True)
except Exception:
    pass

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    print("ERRO: pyserial não instalado. Execute: pip install pyserial")
    sys.exit(1)

try:
    import speech_recognition as sr
except ImportError:
    print("ERRO: SpeechRecognition não instalado. Execute: pip install SpeechRecognition")
    sys.exit(1)

recognizer = sr.Recognizer()

# Configurações do Antigravity CLI e Caminhos do Windows
AGY_BIN = r"C:\Users\Jvrib\AppData\Local\agy\bin\agy.EXE"
AGY_MODEL = "gemini-3.8-flash-low"
CHROME_PATH = r"C:\Program Files\Google\Chrome\Application\chrome.exe"
CODE_CMD = r"C:\Users\Jvrib\AppData\Local\Programs\cursor\resources\app\codeBin\code.cmd"
CODEX_DIR = r"C:\Users\Jvrib\Documents\Codex"

current_agent = "AGY"
active_port = "COM3"
baud_rate = 115200
ser_global = None
ser_lock = threading.Lock()
m5_client_ip = None

try:
    import computer_use_agent
except Exception as e:
    print(f"[AVISO]: Módulo computer_use_agent não carregado: {e}")
    computer_use_agent = None

def send_progress_to_m5(title, body):
    """Envia notificação de progresso intermediária para a tela do M5Stick."""
    global ser_global, ser_lock
    prog = {
        "type": "RESULT",
        "agent": "Computer Use",
        "title": title[:18],
        "text": "",
        "body": body[:50],
        "auto_resume": True
    }
    prog_json = json.dumps(prog, ensure_ascii=False, separators=(',', ':')) + "\n"
    with ser_lock:
        if ser_global and ser_global.is_open:
            try:
                ser_global.write(prog_json.encode('utf-8'))
            except Exception:
                pass

def trigger_m5_ir(endpoint, params):
    """Dispara comandos infravermelho no hardware do M5StickC Plus 2 via HTTP como redundância."""
    global m5_client_ip
    if not m5_client_ip:
        return False, "IP do M5Stick desconhecido"
    try:
        import urllib.request
        qs = urllib.parse.urlencode(params)
        url = f"http://{m5_client_ip}{endpoint}?{qs}"
        req = urllib.request.Request(url, data=b"", method='POST')
        with urllib.request.urlopen(req, timeout=2.0) as resp:
            body = resp.read().decode('utf-8', errors='ignore')
            return True, body
    except Exception as e:
        return False, str(e)

def get_lan_ip():
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(('8.8.8.8', 80))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except Exception:
        return '192.168.0.2'

def find_m5_port():
    ports = serial.tools.list_ports.comports()
    for p in ports:
        if "CH9102" in p.description or "USB-SERIAL" in p.description or "CP210" in p.description:
            return p.device
    for p in ports:
        if p.device == "COM3":
            return "COM3"
    return "COM3"

def query_agy_agent(prompt):
    """Encaminha comandos e perguntas para o Antigravity (agy)."""
    global current_agent
    current_agent = "AGY"
    print(f"\n[AGY CONSULTA]: Consultando Antigravity CLI...")
    print(f"[PROMPT]: {prompt}")

    agy_prompt = (
        f"[Sistema M5StickC Plus 2 - Assistente de Voz]: "
        f"O usuário falou pelo microfone: \"{prompt}\". "
        f"O M5Stick possui emissor Infravermelho FÍSICO (GPIO 19) para Ar Samsung e TV Samsung. "
        f"Se o comando pedir para controlar o ar ou TV, inclua no início da resposta a tag correspondente: "
        f"[IR:AC_POWER], [IR:AC_TEMP_UP], [IR:AC_TEMP_DOWN], [IR:TV_POWER], [IR:TV_VOL_UP], [IR:TV_VOL_DOWN], [IR:TV_MUTE]. "
        f"Se for uma ação no PC, execute-a ou responda de forma concisa. "
        f"Responda sempre em português em no máximo 2 frases curtas para o visor 240x135: "
        f"{prompt}"
    )

    cmd = [
        AGY_BIN,
        "--model", AGY_MODEL,
        "--effort", "low",
        "--dangerously-skip-permissions",
        "-p", agy_prompt
    ]

    try:
        res = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=20,
            encoding='utf-8',
            errors='replace'
        )

        stdout = (res.stdout or "").strip()
        stderr = (res.stderr or "").strip()

        if stdout:
            clean_reply = re.sub(r'\[([^\]]+)\]\([^\)]+\)', r'\1', stdout)
            clean_reply = clean_reply.replace('`', '')
            clean_lines = [l.strip() for l in clean_reply.splitlines() if l.strip()]
            clean_reply = " ".join(clean_lines)

            ir_tag = None
            m_ir = re.search(r'\[IR:([A-Z_]+)\]', clean_reply)
            if m_ir:
                ir_tag = m_ir.group(1)
                clean_reply = re.sub(r'\[IR:[A-Z_]+\]', '', clean_reply).strip()

            if len(clean_reply) > 200:
                clean_reply = clean_reply[:197] + "..."

            print(f"[AGY RESPOSTA]: {clean_reply} (IR tag: {ir_tag})")
            return clean_reply, ir_tag
        else:
            if stderr:
                print(f"[AGY STDERR]: {stderr[:120]}")
            return f"Processado via AGY.", None

    except subprocess.TimeoutExpired:
        print("[AGY AVISO]: Timeout de 20s atingido. O comando continua em segundo plano.")
        return "AGY em execucao no PC (processando em segundo plano).", None
    except Exception as e:
        print(f"[AGY ERRO]: {e}")
        return f"Falha ao acionar AGY: {str(e)[:40]}", None

def check_and_strip_wake_word(text):
    """Detecta a presença do gatilho 'Ei M5' / 'M5' / 'Alexa' e extrai o comando limpo."""
    text_clean = text.strip()
    text_lower = text_clean.lower()

    # 1. Padrões no início: "ei m5", "hey m5", "ok m5", "e aí m5", "olá m5", "ou m5", "m5"
    m = re.search(r'^(?:ei|hey|ok|e\s*a[ií]|ou|ola|olá)?\s*m\s*5[\s,:]*', text_clean, flags=re.IGNORECASE)
    if m:
        prompt = text_clean[m.end():].strip()
        return True, prompt

    # 2. Padrão "alexa"
    m_al = re.search(r'^\s*alexa[\s,:]*', text_clean, flags=re.IGNORECASE)
    if m_al:
        prompt = text_clean[m_al.end():].strip()
        return True, prompt

    # 3. Contém "m5" ou "m 5" em qualquer ponto das primeiras 3 palavras
    words = text_lower.split()
    if len(words) > 0 and any(w in ['m5', 'm-5', 'em5'] for w in words[:3]):
        m_word = re.search(r'\b(?:m5|m\s+5|em5)\b[\s,:]*', text_clean, flags=re.IGNORECASE)
        if m_word:
            prompt = text_clean[m_word.end():].strip()
            return True, prompt

    return False, text_clean

def open_browser(url="https://www.google.com"):
    """Abre o navegador de forma 100% garantida no Windows."""
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

def open_vscode(path=None):
    """Abre o VS Code / Cursor no caminho indicado."""
    try:
        cmd = [CODE_CMD]
        if path:
            cmd.append(path)
        if os.path.exists(CODE_CMD):
            subprocess.Popen(cmd, shell=True)
            return True
        else:
            target = f'code "{path}"' if path else 'code'
            subprocess.Popen(target, shell=True)
            return True
    except Exception as e:
        print(f"[ERRO VSCODE]: {e}")
        return False

def parse_voice_command(text):
    """Interpretação semântica com suporte direto a comandos IR para Ar e TV e Ações no PC."""
    global current_agent
    text_clean = text.strip()
    text_lower = text_clean.lower()
    print(f"\n[INTERPRETAÇÃO]: Analisando '{text_clean}'...")

    # ============================================================
    # 1. Comandos de Ar-Condicionado (Hardware IR GPIO 19)
    # ============================================================
    is_ac = bool(re.search(r'\b(?:ar|ar-condicionado|ar\s+condicionado|arcondicionado)\b', text_lower))
    if is_ac:
        if any(w in text_lower for w in ['desliga', 'desligar', 'desligue', 'apaga', 'apagar', 'para', 'parar', 'corta']):
            trigger_m5_ir("/api/ir/ac", {"device": "0", "action": "7"})
            return {
                "type": "RESULT",
                "agent": "Ar Samsung",
                "ir": "AC_POWER",
                "title": "AR DESLIGADO",
                "text": text_clean,
                "body": "Ar condicionado Samsung desligado!",
                "auto_resume": True
            }
        elif any(w in text_lower for w in ['liga', 'ligar', 'ligue', 'inicia', 'iniciar', 'aciona', 'acionar', 'power']):
            trigger_m5_ir("/api/ir/ac", {"device": "0", "action": "7"})
            return {
                "type": "RESULT",
                "agent": "Ar Samsung",
                "ir": "AC_POWER",
                "title": "AR LIGADO",
                "text": text_clean,
                "body": "Ar condicionado Samsung ligado!",
                "auto_resume": True
            }
        elif any(w in text_lower for w in ['aumenta', 'aumentar', 'sobe', 'subir', 'esquenta', 'calor', 'mais quente']):
            trigger_m5_ir("/api/ir/ac", {"device": "0", "action": "1"})
            return {
                "type": "RESULT",
                "agent": "Ar Samsung",
                "ir": "AC_TEMP_UP",
                "title": "TEMP +",
                "text": text_clean,
                "body": "Temperatura do Ar aumentada (+1°C)!",
                "auto_resume": True
            }
        elif any(w in text_lower for w in ['diminui', 'diminuir', 'baixa', 'baixar', 'esfria', 'frio', 'mais frio', 'desce']):
            trigger_m5_ir("/api/ir/ac", {"device": "0", "action": "0"})
            return {
                "type": "RESULT",
                "agent": "Ar Samsung",
                "ir": "AC_TEMP_DOWN",
                "title": "TEMP -",
                "text": text_clean,
                "body": "Temperatura do Ar reduzida (-1°C)!",
                "auto_resume": True
            }
        else:
            trigger_m5_ir("/api/ir/ac", {"device": "0", "action": "7"})
            return {
                "type": "RESULT",
                "agent": "Ar Samsung",
                "ir": "AC_POWER",
                "title": "AR CONDICIONADO",
                "text": text_clean,
                "body": "Power do Ar Samsung acionado!",
                "auto_resume": True
            }

    # ============================================================
    # 2. Comandos de Televisão (Hardware IR GPIO 19)
    # ============================================================
    is_tv = bool(re.search(r'\b(?:tv|televis[aã]o)\b', text_lower))
    if is_tv:
        if any(w in text_lower for w in ['desliga', 'desligar', 'desligue', 'liga', 'ligar', 'ligue', 'power']):
            trigger_m5_ir("/api/ir/tv", {"device": "0", "cmd": "0"})
            return {
                "type": "RESULT",
                "agent": "TV Samsung",
                "ir": "TV_POWER",
                "title": "TV SAMSUNG",
                "text": text_clean,
                "body": "Power da TV Samsung enviado!",
                "auto_resume": True
            }
        elif any(w in text_lower for w in ['mudo', 'mutar', 'sem som', 'silencio', 'tira o som']):
            trigger_m5_ir("/api/ir/tv", {"device": "0", "cmd": "1"})
            return {
                "type": "RESULT",
                "agent": "TV Samsung",
                "ir": "TV_MUTE",
                "title": "TV MUDO",
                "text": text_clean,
                "body": "Mudo da TV Samsung acionado!",
                "auto_resume": True
            }
        elif any(w in text_lower for w in ['aumenta', 'aumentar', 'sobe', 'subir', 'mais alto', 'som alto']):
            trigger_m5_ir("/api/ir/tv", {"device": "0", "cmd": "2"})
            return {
                "type": "RESULT",
                "agent": "TV Samsung",
                "ir": "TV_VOL_UP",
                "title": "VOL +",
                "text": text_clean,
                "body": "Volume da TV Samsung aumentado!",
                "auto_resume": True
            }
        elif any(w in text_lower for w in ['diminui', 'diminuir', 'baixa', 'baixar', 'mais baixo', 'abaixa']):
            trigger_m5_ir("/api/ir/tv", {"device": "0", "cmd": "3"})
            return {
                "type": "RESULT",
                "agent": "TV Samsung",
                "ir": "TV_VOL_DOWN",
                "title": "VOL -",
                "text": text_clean,
                "body": "Volume da TV Samsung reduzido!",
                "auto_resume": True
            }

    # ============================================================
    # 3. Agente Autônomo de Computer Use (Controle Total do PC)
    # ============================================================
    if computer_use_agent:
        try:
            cu_res = computer_use_agent.handle_computer_use(text_clean, on_progress=send_progress_to_m5)
            if cu_res:
                return cu_res
        except Exception as e:
            print(f"[ERRO COMPUTER USE]: {e}")

    # ============================================================
    # 4. Despachante de Ações no PC (Execução Nativa Instantânea)
    # ============================================================

    # A) Navegador / Internet / Chrome
    is_browser_request = bool(re.search(r'\b(?:navegador|browser|chrome|google chrome|internet|web)\b', text_lower))
    if is_browser_request and any(w in text_lower for w in ['abrir', 'abre', 'abra', 'iniciar', 'inicia', 'inicie', 'entra', 'entrar']):
        print("[DESPACHO PC]: Abrindo navegador...")
        open_browser("https://www.google.com")
        return {
            "type": "RESULT",
            "agent": "Chrome",
            "title": "NAVEGADOR",
            "text": text_clean,
            "body": "Google Chrome aberto no computador!",
            "auto_resume": True
        }

    # B) YouTube / Vídeo
    if 'youtube' in text_lower or (('video' in text_lower or 'vídeo' in text_lower) and any(w in text_lower for w in ['abrir', 'abre', 'abra', 'tocar', 'toca'])):
        print("[DESPACHO PC]: Abrindo YouTube...")
        open_browser("https://www.youtube.com")
        return {
            "type": "RESULT",
            "agent": "YouTube",
            "title": "YOUTUBE",
            "text": text_clean,
            "body": "YouTube aberto no navegador!",
            "auto_resume": True
        }

    # C) Busca na Web / Google
    m_search = re.search(r'^(?:pesquisar|pesquise|procurar|procure|buscar|busca|pesquisa)\s+(?:por\s+|sobre\s+)?(.+)', text_lower)
    if m_search:
        query = m_search.group(1).strip()
        print(f"[DESPACHO PC]: Pesquisando na web: '{query}'...")
        open_browser(f"https://www.google.com/search?q={urllib.parse.quote(query)}")
        return {
            "type": "RESULT",
            "agent": "Busca Web",
            "title": "BUSCA WEB",
            "text": text_clean,
            "body": f"Pesquisando '{query}' no Google!",
            "auto_resume": True
        }

    # D) Codex / VSCode / Editor
    is_codex = bool(re.search(r'\b(?:codex|projeto\s+codex)\b', text_lower))
    if is_codex:
        print("[DESPACHO PC]: Abrindo pasta Codex no VSCode/Cursor...")
        open_vscode(CODEX_DIR)
        return {
            "type": "RESULT",
            "agent": "Codex",
            "title": "CODEX ATIVADO",
            "text": text_clean,
            "body": "Projeto Codex aberto no VS Code!",
            "auto_resume": True
        }

    is_editor = bool(re.search(r'\b(?:vscode|vs code|visual studio|editor|cursor)\b', text_lower))
    if is_editor and any(w in text_lower for w in ['abrir', 'abre', 'abra', 'iniciar', 'inicia', 'inicie']):
        print("[DESPACHO PC]: Abrindo VS Code/Cursor...")
        open_vscode()
        return {
            "type": "RESULT",
            "agent": "VSCode",
            "title": "VS CODE",
            "text": text_clean,
            "body": "Visual Studio Code aberto no PC!",
            "auto_resume": True
        }

    # E) Terminal / PowerShell
    if any(w in text_lower for w in ['terminal', 'powershell', 'cmd', 'prompt']) and any(w in text_lower for w in ['abrir', 'abre', 'abra', 'iniciar']):
        print("[DESPACHO PC]: Abrindo Terminal...")
        subprocess.Popen("start wt || start powershell", shell=True)
        return {
            "type": "RESULT",
            "agent": "Terminal",
            "title": "TERMINAL",
            "text": text_clean,
            "body": "Windows Terminal aberto!",
            "auto_resume": True
        }

    # F) Bloco de Notas / Notepad
    if any(w in text_lower for w in ['bloco de notas', 'notepad']) and any(w in text_lower for w in ['abrir', 'abre', 'abra']):
        print("[DESPACHO PC]: Abrindo Bloco de Notas...")
        subprocess.Popen("notepad", shell=True)
        return {
            "type": "RESULT",
            "agent": "Notepad",
            "title": "BLOCO DE NOTAS",
            "text": text_clean,
            "body": "Bloco de Notas aberto!",
            "auto_resume": True
        }

    # G) Calculadora
    if any(w in text_lower for w in ['calculadora', 'calc']) and any(w in text_lower for w in ['abrir', 'abre', 'abra']):
        print("[DESPACHO PC]: Abrindo Calculadora...")
        subprocess.Popen("calc", shell=True)
        return {
            "type": "RESULT",
            "agent": "Calculadora",
            "title": "CALCULADORA",
            "text": text_clean,
            "body": "Calculadora aberta!",
            "auto_resume": True
        }

    # H) Spotify / Música
    if any(w in text_lower for w in ['spotify', 'tocar musica', 'tocar música']) or (('música' in text_lower or 'musica' in text_lower) and any(w in text_lower for w in ['abrir', 'toca', 'tocar', 'iniciar'])):
        print("[DESPACHO PC]: Abrindo Spotify...")
        subprocess.Popen("start spotify:", shell=True)
        return {
            "type": "RESULT",
            "agent": "Spotify",
            "title": "SPOTIFY",
            "text": text_clean,
            "body": "Spotify aberto no computador!",
            "auto_resume": True
        }

    # I) Explorador de Arquivos
    if any(w in text_lower for w in ['pastas', 'arquivos', 'explorer', 'meus arquivos', 'documentos']) and any(w in text_lower for w in ['abrir', 'abre', 'abra']):
        print("[DESPACHO PC]: Abrindo Explorer...")
        subprocess.Popen("explorer", shell=True)
        return {
            "type": "RESULT",
            "agent": "Arquivos",
            "title": "EXPLORADOR",
            "text": text_clean,
            "body": "Explorador de Arquivos aberto!",
            "auto_resume": True
        }

    # J) Horário e Data
    if any(k == text_lower or text_lower.startswith(k) for k in ['que horas', 'que horas sao', 'que horas são', 'hora certa', 'hora atual', 'data de hoje', 'que dia e hoje', 'que dia é hoje']):
        return {
            "type": "RESULT",
            "agent": "Relógio",
            "title": "HORA ATUAL",
            "text": text_clean,
            "body": f"Agora são {time.strftime('%H:%M:%S')} ({time.strftime('%d/%m/%Y')}).",
            "auto_resume": True
        }

    # ============================================================
    # 4. Inteligência e Raciocínio via Antigravity CLI (agy)
    # ============================================================
    agy_reply, ir_tag = query_agy_agent(text_clean)
    res = {
        "type": "RESULT",
        "agent": "AGY",
        "title": "AGY ATIVO",
        "text": text_clean,
        "body": agy_reply,
        "auto_resume": True
    }
    if ir_tag:
        res["ir"] = ir_tag
    return res

def process_audio_pcm(raw_pcm, sample_rate=16000, mode='ALEXA'):
    """Processa pacote de áudio PCM, realiza STT e encaminha para execução."""
    if not raw_pcm or len(raw_pcm) < 800:
        print("[AVISO]: Áudio muito curto ou vazio recebido.")
        if mode == 'ALEXA':
            return {"type": "IGNORE"}
        return {
            "type": "RESULT",
            "agent": "Voz",
            "title": "AUDIO MUITO CURTO",
            "text": "",
            "body": "Fale mais próximo ao microfone por pelo menos 1 segundo.",
            "auto_resume": False
        }

    audio_np = np.frombuffer(raw_pcm, dtype=np.int16)
    peak = int(np.max(np.abs(audio_np)))

    # Normalização Automática de Ganho (AGC)
    if peak > 30 and peak < 20000:
        gain = 25000.0 / float(peak)
        audio_boosted = np.clip(audio_np.astype(np.float32) * gain, -32768, 32767).astype(np.int16)
    else:
        audio_boosted = audio_np

    audio_data = sr.AudioData(audio_boosted.tobytes(), sample_rate, 2)

    try:
        text = recognizer.recognize_google(audio_data, language="pt-BR")
        print(f"\n[RECONHECIDO]: '{text}'")

        if mode == 'ALEXA':
            has_wake, prompt = check_and_strip_wake_word(text)
            if not has_wake:
                print(f"[WAKE WORD]: Descartado ruído/conversa sem 'Ei M5': '{text}'")
                return {"type": "IGNORE"}

            print(f"[WAKE WORD DETECTADO!]: Prompt extraído = '{prompt}'")
            if not prompt:
                ack_res = {
                    "type": "RESULT",
                    "agent": "M5",
                    "title": "SIM, ESTOU OUVINDO",
                    "text": "Ei M5!",
                    "body": "Estou ouvindo! O que você deseja?",
                    "auto_resume": True
                }
                res_json = json.dumps(ack_res, ensure_ascii=False, separators=(',', ':')) + "\n"
                with ser_lock:
                    if ser_global and ser_global.is_open:
                        ser_global.write(res_json.encode('utf-8'))
                return ack_res
            text_to_process = prompt
        else:
            text_to_process = text

        trans_msg = json.dumps({"type": "TRANS", "text": text_to_process}, ensure_ascii=False) + "\n"
        with ser_lock:
            if ser_global and ser_global.is_open:
                ser_global.write(trans_msg.encode('utf-8'))

        res = parse_voice_command(text_to_process)
        print(f"[RESPOSTA M5]: {res.get('title')} -> {res.get('body')} (IR: {res.get('ir')})")

        res_json = json.dumps(res, ensure_ascii=False, separators=(',', ':')) + "\n"
        with ser_lock:
            if ser_global and ser_global.is_open:
                ser_global.write(res_json.encode('utf-8'))

        return res

    except sr.UnknownValueError:
        if mode == 'ALEXA':
            return {"type": "IGNORE"}
        err_res = {
            "type": "RESULT",
            "agent": "AGY",
            "title": "NAO ENTENDI",
            "text": "",
            "body": "Não compreendido. Fale próximo ao microfone.",
            "auto_resume": False
        }
        with ser_lock:
            if ser_global and ser_global.is_open:
                ser_global.write((json.dumps(err_res, ensure_ascii=False) + "\n").encode('utf-8'))
        return err_res

    except Exception as e:
        print(f"[ERRO STT]: {e}")
        if mode == 'ALEXA':
            return {"type": "IGNORE"}
        err_res = {
            "type": "RESULT",
            "agent": "Erro",
            "title": "ERRO PROCESSAMENTO",
            "text": "",
            "body": str(e)[:45],
            "auto_resume": False
        }
        with ser_lock:
            if ser_global and ser_global.is_open:
                ser_global.write((json.dumps(err_res, ensure_ascii=False) + "\n").encode('utf-8'))
        return err_res

class AudioHTTPHandler(http.server.BaseHTTPRequestHandler):
    def log_message(self, format, *args):
        pass

    def do_POST(self):
        global m5_client_ip
        m5_client_ip = self.client_address[0]
        if self.path == '/audio':
            content_length = int(self.headers.get('Content-Length', 0))
            mode = self.headers.get('X-Voice-Mode', 'ALEXA')
            raw_pcm = self.rfile.read(content_length)
            print(f"\n[HTTP /audio]: Recebidos {len(raw_pcm)} bytes PCM (modo={mode}) de {m5_client_ip}")
            result = process_audio_pcm(raw_pcm, 16000, mode=mode)

            self.send_response(200)
            self.send_header('Content-Type', 'application/json; charset=utf-8')
            self.end_headers()
            self.wfile.write(json.dumps(result, ensure_ascii=False, separators=(',', ':')).encode('utf-8'))
        else:
            self.send_response(404)
            self.end_headers()

    def do_GET(self):
        if self.path == '/ping':
            self.send_response(200)
            self.send_header('Content-Type', 'text/plain')
            self.end_headers()
            self.wfile.write(b"OK")
        else:
            self.send_response(404)
            self.end_headers()

def start_http_server(host, port):
    def run_server():
        try:
            server = http.server.ThreadingHTTPServer(('0.0.0.0', port), AudioHTTPHandler)
            print(f"[HTTP SERVER]: Ativo em http://{host}:{port}/audio")
            server.serve_forever()
        except Exception as e:
            print(f"[HTTP SERVER ERRO]: {e}")

    t = threading.Thread(target=run_server, daemon=True)
    t.start()

def main():
    global active_port, ser_global
    lan_ip = get_lan_ip()
    port_http = 5000

    start_http_server(lan_ip, port_http)

    port = find_m5_port()
    active_port = port

    print("=================================================================")
    print("  M5StickC Plus 2 - Ponte de Voz & Computer Use v8.0")
    print(f"  Porta Serial: {port} @ {baud_rate} baud")
    print(f"  Servidor HTTP de Voz: http://{lan_ip}:{port_http}/audio")
    print("  Modo Mãos-Livres (Wake-Word): 'Ei M5, [comando]'")
    print("  Computer Use Nativo: Mouse, Teclado, Mídia, Visão, Janelas")
    print("  Hardware IR Ativo: GPIO 19 (Ar Samsung / TV Samsung)")
    print("=================================================================")
    print("Pronto! Diga 'Ei M5' diretamente para o seu M5Stick...\n")

    was_connected = False
    while True:
        try:
            ser = serial.Serial(port, baud_rate, timeout=0.1)
            with ser_lock:
                ser_global = ser
            print(f"[SERIAL] Conectado na porta {port}!")
            was_connected = True

            time.sleep(1.0)
            cfg_msg = json.dumps({"type": "CONFIG", "ip": lan_ip, "port": port_http}) + "\n"
            ser.write(cfg_msg.encode('utf-8'))
            time.sleep(0.1)
            ready_msg = json.dumps({"type": "READY", "agent": "AGY (Sessao)"}) + "\n"
            ser.write(ready_msg.encode('utf-8'))

            raw_audio_buffer = bytearray()
            expected_audio_bytes = 0

            while True:
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                if line:
                    print(f"[M5]: {line}")

                    if line.startswith("VOICE_AUDIO "):
                        try:
                            parts = line.split()
                            expected_audio_bytes = int(parts[1])
                            mode = parts[2] if len(parts) > 2 else "ALEXA"
                            print(f"[SERIAL AUDIO]: Recebendo {expected_audio_bytes} bytes via serial (modo={mode})...")
                            raw_audio_buffer = ser.read(expected_audio_bytes)
                            print(f"[SERIAL AUDIO]: {len(raw_audio_buffer)} bytes recebidos com sucesso!")
                            res = process_audio_pcm(bytes(raw_audio_buffer), 16000, mode=mode)
                            ser.write((json.dumps(res, ensure_ascii=False) + "\n").encode('utf-8'))
                        except Exception as e:
                            print(f"[SERIAL AUDIO ERRO]: {e}")

                time.sleep(0.01)

        except (serial.SerialException, FileNotFoundError, PermissionError) as e:
            if was_connected:
                print(f"[AVISO] Porta {port} desconectada. Aguardando reconexão...")
                was_connected = False
            with ser_lock:
                ser_global = None
            time.sleep(2.5)
        except Exception as e:
            print(f"[ERRO]: {e}")
            time.sleep(2)

if __name__ == '__main__':
    main()
