# -*- coding: utf-8 -*-
"""
M5StickC Plus 2 - Ponte de Voz & Agentes de IA para PC
Captura áudio via sounddevice (microfone do PC/Headset Corsair) e responde ao M5Stick.
"""

import os
import sys
import time
import re
import json
import subprocess
import threading
import queue
import numpy as np

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    print("ERRO: pyserial não instalado. Execute: pip install pyserial")
    sys.exit(1)

try:
    import sounddevice as sd
except ImportError:
    print("ERRO: sounddevice não instalado. Execute: pip install sounddevice")
    sys.exit(1)

try:
    import speech_recognition as sr
except ImportError:
    print("ERRO: SpeechRecognition não instalado. Execute: pip install SpeechRecognition")
    sys.exit(1)

# Configuração do Recognizer
recognizer = sr.Recognizer()
recognizer.energy_threshold = 250

current_agent = "IA Geral"
active_port = "COM3"
baud_rate = 115200
SAMPLE_RATE = 16000

is_recording = False
stop_recording_event = threading.Event()
audio_frames = []

def find_m5_port():
    ports = serial.tools.list_ports.comports()
    for p in ports:
        if "CH9102" in p.description or "USB-SERIAL" in p.description or "CP210" in p.description:
            return p.device
    for p in ports:
        if p.device == "COM3":
            return "COM3"
    return "COM3"

def parse_voice_command(text):
    global current_agent
    text_clean = text.strip()
    text_lower = text_clean.lower()

    # Intenção 1: "abrir <agente/app>", "iniciar <app>", "trocar para <agente>"
    m_open = re.search(r'^(?:abrir|iniciar|executar|trocar para|chamar)\s+(?:o\s+|a\s+)?(?:agente\s+(?:de\s+)?|ferramenta\s+(?:de\s+)?|aplicativo\s+(?:de\s+)?|app\s+)?(.+)', text_lower)
    if m_open:
        target = m_open.group(1).strip()
        if any(w in target for w in ['codigo', 'código', 'vscode', 'vs code', 'visual studio', 'desenvolvimento', 'editor']):
            current_agent = "VSCode"
            subprocess.Popen("code", shell=True)
            return {
                "type": "RESULT",
                "agent": "VSCode",
                "title": "AGENTE ATIVADO",
                "body": "Visual Studio Code aberto no PC!"
            }
        if any(w in target for w in ['navegador', 'chrome', 'internet', 'web', 'browser', 'pesquisa']):
            current_agent = "Navegador"
            subprocess.Popen("start chrome", shell=True)
            return {
                "type": "RESULT",
                "agent": "Navegador",
                "title": "AGENTE ATIVADO",
                "body": "Google Chrome aberto no PC!"
            }
        if any(w in target for w in ['terminal', 'powershell', 'cmd', 'console', 'prompt', 'bash']):
            current_agent = "Terminal"
            subprocess.Popen("start wt || start powershell", shell=True)
            return {
                "type": "RESULT",
                "agent": "Terminal",
                "title": "AGENTE ATIVADO",
                "body": "Windows Terminal aberto no PC!"
            }
        if any(w in target for w in ['notepad', 'bloco de notas', 'notas', 'anotacoes', 'anotações']):
            current_agent = "Notepad"
            subprocess.Popen("notepad", shell=True)
            return {
                "type": "RESULT",
                "agent": "Notepad",
                "title": "AGENTE ATIVADO",
                "body": "Bloco de Notas aberto no PC!"
            }
        if any(w in target for w in ['calculadora', 'calc', 'financas', 'finanças']):
            current_agent = "Calculadora"
            subprocess.Popen("calc", shell=True)
            return {
                "type": "RESULT",
                "agent": "Calculadora",
                "title": "AGENTE ATIVADO",
                "body": "Calculadora aberta no PC!"
            }
        if any(w in target for w in ['spotify', 'musica', 'música', 'som']):
            current_agent = "Spotify"
            subprocess.Popen("start spotify:", shell=True)
            return {
                "type": "RESULT",
                "agent": "Spotify",
                "title": "AGENTE ATIVADO",
                "body": "Spotify aberto no PC!"
            }
        if any(w in target for w in ['discord', 'chat']):
            current_agent = "Discord"
            subprocess.Popen("start discord:", shell=True)
            return {
                "type": "RESULT",
                "agent": "Discord",
                "title": "AGENTE ATIVADO",
                "body": "Discord aberto no PC!"
            }

        subprocess.Popen(f"start {target}", shell=True)
        current_agent = target.title()
        return {
            "type": "RESULT",
            "agent": current_agent,
            "title": "AGENTE ATIVADO",
            "body": f"Comando '{target}' executado no PC!"
        }

    # Intenção 2: Pergunta / Comando para a IA
    ai_reply = query_ai_agent(text_clean)
    return {
        "type": "RESULT",
        "agent": current_agent,
        "title": "RESPOSTA IA",
        "body": ai_reply
    }

def query_ai_agent(prompt):
    api_key = os.environ.get("GEMINI_API_KEY")
    if api_key:
        try:
            from google import genai
            client = genai.Client(api_key=api_key)
            resp = client.models.generate_content(
                model="gemini-2.5-flash",
                contents=prompt,
                config={"system_instruction": "Você é um assistente de IA conciso respondendo para uma tela pequena de 135x240 pixels. Responda em no máximo 2 a 3 frases diretas em português sem enrolação."}
            )
            if resp and resp.text:
                return resp.text.strip()
        except Exception as e:
            print(f"[IA] Erro na API Gemini: {e}")

    p_low = prompt.lower()
    if "hora" in p_low or "que horas" in p_low:
        return f"Agora são {time.strftime('%H:%M:%S')}."
    if "data" in p_low or "que dia" in p_low:
        return f"Hoje é {time.strftime('%d/%m/%Y')}."
    if "clima" in p_low or "temperatura" in p_low or "tempo" in p_low:
        return "Consulte a aba Clima do M5Stick sincronizada com a internet."
    if "quem e voce" in p_low or "quem é você" in p_low or "seu nome" in p_low:
        return "Sou o Agente IA do seu M5Stick integrado ao Windows PC!"

    # Consulta rápida DuckDuckGo
    try:
        import urllib.parse
        import requests
        q_enc = urllib.parse.quote(prompt)
        url = f"https://api.duckduckgo.com/?q={q_enc}&format=json&no_html=1&skip_disambig=1"
        res = requests.get(url, timeout=3).json()
        if res.get("AbstractText"):
            return res["AbstractText"][:200]
    except Exception:
        pass

    # Consulta Wikipedia PT
    try:
        import urllib.parse
        import requests
        first_word = prompt.split()[-1] if len(prompt.split()) > 1 else prompt
        w_enc = urllib.parse.quote(first_word)
        r = requests.get(f"https://pt.wikipedia.org/api/rest_v1/page/summary/{w_enc}", timeout=3)
        if r.status_code == 200:
            extract = r.json().get("extract", "")
            if extract:
                return extract[:200]
    except Exception:
        pass

    return f"Comando '{prompt}' processado pelo agente '{current_agent}'."

def record_audio_stream(ser):
    global is_recording, audio_frames
    audio_frames = []
    print("\n[AUDIO] Iniciando gravação pelo microfone do PC...")

    def audio_callback(indata, frame_count, time_info, status):
        if is_recording:
            audio_frames.append(indata.copy())

    try:
        with sd.InputStream(samplerate=SAMPLE_RATE, channels=1, dtype='int16', callback=audio_callback):
            print("[AUDIO] Microfone ativo! Fale seu comando agora...")
            start_time = time.time()
            while not stop_recording_event.is_set() and (time.time() - start_time < 12.0):
                time.sleep(0.02)

        print(f"[AUDIO] Gravação finalizada. Blocos capturados: {len(audio_frames)}")
        if not audio_frames:
            ser.write(b'{"type":"RESULT","agent":"IA Geral","title":"TIMEOUT","body":"Nenhum som capturado. Tente novamente."}\n')
            return

        raw_pcm = np.concatenate(audio_frames, axis=0).tobytes()
        audio_data = sr.AudioData(raw_pcm, SAMPLE_RATE, 2)

        print("[AUDIO] Transcrevendo via Google Speech...")
        ser.write(b'{"type":"STATUS","msg":"TRANSCREVENDO..."}\n')

        text = recognizer.recognize_google(audio_data, language="pt-BR")
        print(f"[RECONHECIDO]: '{text}'")

        ser.write(f'{{"type":"TRANS","text":"{text}"}}\n'.encode('utf-8'))
        time.sleep(0.1)

        res = parse_voice_command(text)
        print(f"[RESPOSTA]: {res['title']} -> {res['body']}")

        res_json = json.dumps(res, ensure_ascii=False)
        ser.write((res_json + "\n").encode('utf-8'))

    except sr.UnknownValueError:
        print("[AUDIO] Não foi possível compreender a fala.")
        ser.write(b'{"type":"RESULT","agent":"IA Geral","title":"INCERTO","body":"Nao entendi o audio. Fale novamente."}\n')
    except Exception as e:
        print(f"[ERRO]: {e}")
        ser.write(f'{{"type":"RESULT","agent":"Erro","title":"ERRO","body":"{str(e)[:50]}"}}\n'.encode('utf-8'))
    finally:
        is_recording = False

def main():
    global active_port, is_recording, stop_recording_event
    active_port = find_m5_port()
    print("=" * 60)
    print("  M5StickC Plus 2 - Ponte de Voz & Agentes de IA para PC")
    print(f"  Porta Serial: {active_port} @ {baud_rate} baud")
    print("  Microfone: Corsair Headset / Microfone Padrão do Windows")
    print("=" * 60)
    print("Aguardando comandos do M5StickC Plus 2 (Segure Botão A no menu 'Agente IA')...\n")

    try:
        ser = serial.Serial(active_port, baud_rate, timeout=0.1)
    except Exception as e:
        print(f"ERRO: Não foi possível abrir a porta {active_port}: {e}")
        return

    time.sleep(1.0)
    ser.write(b'{"type":"READY","agent":"IA Geral"}\n')

    buffer = ""
    while True:
        try:
            if ser.in_waiting:
                chunk = ser.read(ser.in_waiting).decode('utf-8', errors='ignore')
                buffer += chunk
                while '\n' in buffer:
                    line, buffer = buffer.split('\n', 1)
                    line = line.strip()
                    if not line:
                        continue

                    print(f"[M5]: {line}")

                    if "VOICE_START" in line:
                        print("[TRIGGER]: Botão A pressionado! Ouvindo microfone...")
                        stop_recording_event.clear()
                        if not is_recording:
                            is_recording = True
                            t = threading.Thread(target=record_audio_stream, args=(ser,))
                            t.daemon = True
                            t.start()

                    elif "VOICE_STOP" in line:
                        print("[TRIGGER]: Botão A solto. Parando gravação...")
                        stop_recording_event.set()

            time.sleep(0.02)
        except KeyboardInterrupt:
            print("\nEncerrando ponte de voz...")
            break
        except Exception as e:
            print(f"[AVISO SERIAL]: {e}")
            time.sleep(0.5)

if __name__ == "__main__":
    main()
