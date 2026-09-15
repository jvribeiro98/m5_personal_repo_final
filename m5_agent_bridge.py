# -*- coding: utf-8 -*-
"""Voice bridge for M5StickC Plus2: one result on the originating transport.

LAN access is opt-in: M5_BRIDGE_HOST and M5_DEVICE_IP (or M5_IP_CACHE_FILE).
The serial connection is owned here. Importing this module starts no services.
"""
import base64
import binascii
from collections import deque
import http.server
import ipaddress
import json
import os
import re
import shutil
import socket
import subprocess
import sys
import threading
import time
import unicodedata
import urllib.parse
import urllib.request
import webbrowser

import numpy as np
try:
    import serial
    import serial.tools.list_ports
except ImportError:
    serial = None
try:
    import speech_recognition as sr
except ImportError:
    sr = None
try:
    import computer_use_agent
except (ImportError, OSError, AttributeError):
    computer_use_agent = None

AGY_BIN = os.environ.get('M5_AGY_BIN', shutil.which('agy') or os.path.join(
    os.environ.get('LOCALAPPDATA', os.path.expanduser('~')), 'agy', 'bin', 'agy.exe'))
AGY_MODEL = os.environ.get('M5_AGY_MODEL', '')
CHROME_PATH = os.environ.get('M5_CHROME_PATH', os.path.join(
    os.environ.get('PROGRAMFILES', r'C:\Program Files'), 'Google', 'Chrome', 'Application', 'chrome.exe'))
CODE_CMD = os.environ.get('M5_CODE_CMD', shutil.which('cursor') or shutil.which('code') or '')
CODEX_DIR = os.environ.get('M5_CODEX_DIR', os.path.join(os.path.expanduser('~'), 'Documents', 'Codex'))
IP_CACHE_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'm5_device_ip.json')
MAX_AUDIO_BYTES = 960000
baud_rate = 115200
active_port = None
ser_global = None
ser_lock = threading.Lock()
processing_lock = threading.Lock()
current_agent = 'AGY'
m5_client_ip = None
recognizer = sr.Recognizer() if sr else None
IR_COMMANDS = frozenset(('AC_ON', 'AC_OFF', 'AC_POWER', 'AC_TEMP_UP', 'AC_TEMP_DOWN',
                         'TV_POWER', 'TV_VOL_UP', 'TV_VOL_DOWN', 'TV_MUTE'))


def number_setting(name, default, minimum, maximum):
    value = float(os.environ.get(name, str(default)))
    if not minimum <= value <= maximum:
        raise ValueError(f'{name} deve estar entre {minimum} e {maximum}.')
    return value


def configured_device_ip():
    value = os.environ.get('M5_DEVICE_IP', '').strip()
    if not value:
        try:
            with open(os.environ.get('M5_IP_CACHE_FILE', IP_CACHE_FILE), encoding='utf-8') as source:
                value = json.load(source).get('ip', '')
        except (OSError, ValueError, TypeError, AttributeError):
            return None
    try:
        ip = ipaddress.IPv4Address(value)
        return str(ip) if not ip.is_unspecified and not ip.is_multicast and str(ip) != '255.255.255.255' else None
    except (ValueError, TypeError):
        return None


def is_allowed_audio_peer(address):
    try:
        return ipaddress.ip_address(address).is_loopback or address == configured_device_ip()
    except ValueError:
        return False


def encode_result(result):
    """Bound the actual UTF-8 JSON packet, including escaped control characters."""
    packet = {'type': result.get('type', 'RESULT')}
    for key, limit in [('id', 32), ('agent', 40), ('title', 40), ('text', 300), ('body', 1500)]:
        if key in result:
            packet[key] = str(result[key])[:limit]
    if result.get('ir') in IR_COMMANDS:
        packet['ir'] = result['ir']
    if 'auto_resume' in result:
        packet['auto_resume'] = result['auto_resume'] is True
    while True:
        encoded = json.dumps(packet, ensure_ascii=False, separators=(',', ':')).encode('utf-8')
        if len(encoded) <= 8191:
            return encoded
        for key in ('body', 'text'):
            if packet.get(key):
                packet[key] = packet[key][:len(packet[key]) // 2]
                break


def write_serial_result(port, result):
    payload = encode_result(result) + b'\n'
    with ser_lock:
        if not port or not port.is_open:
            raise OSError('Conexão serial encerrada.')
        written = port.write(payload)
        if written != len(payload):
            raise OSError('Resposta serial incompleta; não será repetida automaticamente.')


def send_progress_to_m5(title, body):
    if ser_global:
        write_serial_result(ser_global, {'type': 'PROGRESS', 'title': title, 'body': body})


def trigger_m5_ir(endpoint, params):
    """Explicit HTTP helper; voice dispatch only returns a tag, never calls this."""
    ip = configured_device_ip()
    if not ip:
        return False, 'IP do M5 desconhecido'
    try:
        url = f'http://{ip}{endpoint}?' + urllib.parse.urlencode(params)
        request = urllib.request.Request(url, data=b'', method='POST')
        with urllib.request.urlopen(request, timeout=3) as response:
            result = json.loads(response.read(65536).decode('utf-8'))
        return result.get('ok') is True, result.get('message', '')
    except Exception as error:
        return False, str(error)


def get_lan_ip():
    advertised = os.environ.get('M5_BRIDGE_ADVERTISE_IP', '').strip()
    if advertised:
        return str(ipaddress.IPv4Address(advertised))
    target = configured_device_ip()
    if target:
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
                sock.connect((target, 80))
                return sock.getsockname()[0]
        except OSError:
            pass
    candidates = {entry[4][0] for entry in socket.getaddrinfo(socket.gethostname(), None, socket.AF_INET)}
    candidates = {ip for ip in candidates if not ipaddress.ip_address(ip).is_loopback}
    return next(iter(candidates)) if len(candidates) == 1 else None


def find_m5_port():
    configured = os.environ.get('M5_SERIAL_PORT', '').strip()
    if configured.lower() in ('none', 'off', 'disabled'):
        return None
    if configured:
        return configured
    if serial is None:
        return None
    matches = [port.device for port in serial.tools.list_ports.comports()
               if any(name in (port.description or '').upper() for name in ('CH9102', 'CP210', 'M5STACK'))]
    return matches[0] if len(matches) == 1 else None


def query_agy_agent(prompt):
    timeout = number_setting('M5_AGY_TIMEOUT', 30, 1, 60)
    instruction = (
        'Assistente do M5Stick. Responda em português em até 2 frases. '
        'Somente responda à pergunta; não execute ferramentas ou ações no computador. '
        'Comandos de dispositivo e computador são tratados localmente pela ponte. '
        'Não inclua tags IR. Pedido: ' + prompt)
    command = [AGY_BIN]
    if AGY_MODEL:
        command += ['--model', AGY_MODEL]
    command += ['-p', instruction]
    try:
        response = subprocess.run(command, capture_output=True, text=True,
                                  timeout=timeout, encoding='utf-8', errors='replace')
        if response.returncode != 0:
            return f'Falha no AGY (código {response.returncode}). Consulte o terminal.', None
        reply = (response.stdout or '').strip()
        if not reply:
            return 'AGY não retornou uma resposta.', None
        reply = re.sub(r'\[IR:[A-Z_]+\]', '', reply)
        reply = re.sub(r'\[([^\]]+)\]\([^\)]+\)', r'\1', reply).replace('`', '')
        return ' '.join(reply.split())[:1500], None
    except subprocess.TimeoutExpired:
        return f'AGY excedeu o limite de {timeout:g}s; a chamada foi encerrada.', None
    except (OSError, ValueError) as error:
        return f'Falha ao acionar AGY: {str(error)[:120]}', None


def check_and_strip_wake_word(text):
    cleaned = text.strip()
    match = re.match(r'(?:(?:ei|hey|ok|e\s*a[ií]|ou|ol[aá])\s+)?(?:m[ -]?5|em5|alexa)\b[\s,:.!-]*', cleaned, re.I)
    if match:
        return True, cleaned[match.end():].strip()
    return False, cleaned


def open_browser(url='https://www.google.com'):
    try:
        if os.path.isfile(CHROME_PATH):
            subprocess.Popen([CHROME_PATH, url])
            return True
        return bool(webbrowser.open(url))
    except OSError:
        return False


def open_vscode(path=None):
    if not CODE_CMD:
        return False
    try:
        subprocess.Popen([CODE_CMD] + ([path] if path else []))
        return True
    except OSError:
        return False


def _result(text, title, body, agent='M5', success=True, ir=None):
    result = {'type': 'RESULT', 'agent': agent, 'title': title, 'text': text,
              'body': body, 'auto_resume': success}
    if ir:
        result['ir'] = ir
    return result


def parse_voice_command(text, on_progress=None):
    cleaned = text.strip()
    normalized = ''.join(c for c in unicodedata.normalize('NFD', cleaned.lower())
                         if unicodedata.category(c) != 'Mn').strip(' .!?')
    if not normalized:
        return _result(cleaned, 'SEM COMANDO', 'Diga o que deseja fazer.', success=False)
    if re.match(r'^(?:nao|nunca|jamais)\b', normalized):
        return _result(cleaned, 'SEM ACAO', 'Nenhuma ação executada.')
    # Imperative, anchored commands prevent questions or dictated text firing IR.
    if re.match(r'^(?:ligue|liga|ligar|ative|ativa|desligue|desliga|desligar|apague|apaga|aumente|aumenta|aumentar|diminua|diminui|diminuir|abaixe|abaixa|baixe|baixa|reduza|suba|sobe|mute|mutar|silencie)\b', normalized):
        ac = re.search(r'\b(?:ar(?: condicionado)?|ar-condicionado|arcondicionado)\b', normalized)
        tv = re.search(r'\b(?:tv|televisao)\b', normalized)
        off = re.match(r'^(?:deslig|apagu|apaga)', normalized)
        on = re.match(r'^(?:lig|ativ)', normalized)
        up = re.match(r'^(?:aument|suba|sobe)', normalized)
        down = re.match(r'^(?:diminu|abaix|baix|reduz)', normalized)
        ir = None
        if ac:
            ir = 'AC_OFF' if off else 'AC_ON' if on else 'AC_TEMP_UP' if up else 'AC_TEMP_DOWN' if down else None
        elif tv:
            ir = 'TV_POWER' if off or on else 'TV_VOL_UP' if up else 'TV_VOL_DOWN' if down else 'TV_MUTE' if re.match(r'^(?:mute|mutar|silencie)', normalized) else None
        if ir:
            return _result(cleaned, 'CONTROLE IR', 'Comando encaminhado ao emissor IR do M5.', 'Infravermelho', ir=ir)
    if computer_use_agent:
        response = computer_use_agent.handle_computer_use(cleaned, on_progress=on_progress)
        if response:
            return response
    opening = re.fullmatch(r'(?:abrir|abre|abra|iniciar|inicia|inicie)\s+(?:o\s+|a\s+)?(.+)', normalized)
    if opening:
        target = opening.group(1)
        if target in ('vscode', 'vs code', 'visual studio code', 'editor', 'cursor', 'codex'):
            success = open_vscode(CODEX_DIR if target == 'codex' else None)
        elif target in ('terminal', 'powershell', 'cmd', 'prompt', 'bloco de notas', 'notepad', 'calculadora', 'calc'):
            executable = {'terminal': shutil.which('wt') or 'powershell.exe', 'powershell': 'powershell.exe',
                          'cmd': 'cmd.exe', 'prompt': 'cmd.exe', 'bloco de notas': 'notepad.exe',
                          'notepad': 'notepad.exe', 'calculadora': 'calc.exe', 'calc': 'calc.exe'}[target]
            try:
                subprocess.Popen([executable])
                success = True
            except OSError:
                success = False
        else:
            success = None
        if success is not None:
            return _result(cleaned, 'APLICATIVO' if success else 'ERRO',
                           f'{target} iniciado.' if success else f'Não foi possível abrir {target}.', success=success)
    if re.match(r'^(?:que horas|hora certa|hora atual|data de hoje|que dia e hoje)\b', normalized):
        return _result(cleaned, 'HORA ATUAL', time.strftime('%H:%M:%S (%d/%m/%Y)'), 'Relógio')
    reply, _ = query_agy_agent(cleaned)
    return _result(cleaned, 'RESPOSTA AGY', reply, 'AGY')


def validate_audio(size, mode):
    if type(size) is not int or size <= 0 or size > MAX_AUDIO_BYTES or size % 2:
        raise ValueError('Áudio deve conter PCM16, com tamanho par entre 2 e 960000 bytes.')
    if mode not in ('ALEXA', 'PTT'):
        raise ValueError('Modo de voz deve ser ALEXA ou PTT.')


def process_audio_pcm(raw_pcm, sample_rate=16000, mode='ALEXA', on_progress=None):
    validate_audio(len(raw_pcm), mode)
    if sample_rate != 16000:
        raise ValueError('Taxa de áudio suportada: 16000Hz.')
    if len(raw_pcm) < 800:
        return {'type': 'IGNORE'} if mode == 'ALEXA' else _result('', 'AUDIO MUITO CURTO', 'Fale por pelo menos um segundo.', success=False)
    if sr is None or recognizer is None:
        return _result('', 'DEPENDENCIA AUSENTE', 'Instale SpeechRecognition para reconhecer voz.', success=False)
    audio = np.frombuffer(raw_pcm, dtype='<i2')
    peak = int(np.max(np.abs(audio.astype(np.int32))))
    if 30 < peak < 20000:
        audio = np.clip(audio.astype(np.float32) * (25000.0 / peak), -32768, 32767).astype('<i2')
    audio_data = sr.AudioData(audio.tobytes(), sample_rate, 2)
    try:
        recognizer.operation_timeout = number_setting('M5_STT_TIMEOUT', 15, 1, 20)
        text = recognizer.recognize_google(audio_data, language='pt-BR')
        if mode == 'ALEXA':
            awake, text = check_and_strip_wake_word(text)
            if not awake:
                return {'type': 'IGNORE'}
            if not text:
                return _result('Ei M5!', 'SIM, ESTOU OUVINDO', 'O que você deseja?')
        return parse_voice_command(text, on_progress=on_progress)
    except sr.UnknownValueError:
        return {'type': 'IGNORE'} if mode == 'ALEXA' else _result('', 'NAO ENTENDI', 'Fale próximo ao microfone.', success=False)
    except Exception as error:
        return _result('', 'ERRO PROCESSAMENTO', str(error)[:150], success=False)


def read_serial_audio(port, expected):
    validate_audio(expected, 'ALEXA')
    deadline = time.monotonic() + 5 + expected * 10 / baud_rate * 1.5
    data = bytearray()
    while len(data) < expected:
        if time.monotonic() >= deadline:
            raise TimeoutError(f'Áudio serial incompleto: {len(data)}/{expected} bytes.')
        data.extend(port.read(min(4096, expected - len(data))))
    return bytes(data)


class SerialAudioAssembler:
    """Bounded JSON/base64 audio frames; unrelated device logs cannot corrupt PCM."""
    def __init__(self):
        self.current = None
        self.finished = deque(maxlen=32)

    def expire(self):
        if self.current and time.monotonic() - self.current['at'] > 10:
            self.finished.append(self.current['id'])
            self.current = None
            raise TimeoutError('Recepção de áudio expirou.')

    def feed(self, message):
        self.expire()
        if not isinstance(message, dict):
            raise ValueError('Frame de áudio deve ser objeto JSON.')
        kind, request_id = message.get('type'), message.get('id')
        if kind not in ('AUDIO_START', 'AUDIO_CHUNK', 'AUDIO_END', 'AUDIO_CANCEL'):
            return None
        if not isinstance(request_id, str) or not re.fullmatch(r'[A-Za-z0-9_-]{1,32}', request_id):
            raise ValueError('ID de áudio inválido.')
        if request_id in self.finished:
            return None
        if kind == 'AUDIO_START':
            validate_audio(message.get('bytes'), message.get('mode'))
            if self.current:
                if self.current['id'] == request_id:
                    raise ValueError('AUDIO_START repetido durante recepção.')
                self.finished.append(self.current['id'])
            self.current = {'id': request_id, 'bytes': message['bytes'], 'mode': message['mode'],
                            'data': bytearray(), 'seq': 0, 'at': time.monotonic()}
            return None
        state = self.current
        if state is None:
            return None
        try:
            if request_id != state['id']:
                raise ValueError('ID não corresponde à captura ativa.')
            if kind == 'AUDIO_CANCEL':
                self.finished.append(request_id)
                self.current = None
            elif kind == 'AUDIO_CHUNK':
                if type(message.get('seq')) is not int or message['seq'] != state['seq']:
                    raise ValueError('Chunk fora de sequência.')
                encoded = message.get('data')
                if not isinstance(encoded, str) or len(encoded) > 344:
                    raise ValueError('Chunk excessivo ou inválido.')
                data = base64.b64decode(encoded, validate=True)
                if not data or len(data) > 256 or len(state['data']) + len(data) > state['bytes']:
                    raise ValueError('Tamanho de chunk inválido.')
                state['data'].extend(data)
                state['seq'] += 1
                state['at'] = time.monotonic()
            elif kind == 'AUDIO_END':
                if len(state['data']) != state['bytes']:
                    raise ValueError('Áudio incompleto; captura descartada.')
                self.finished.append(request_id)
                self.current = None
                return request_id, bytes(state['data']), state['mode']
        except (ValueError, binascii.Error):
            self.finished.append(state['id'])
            self.current = None
            raise
        return None


class AudioHTTPHandler(http.server.BaseHTTPRequestHandler):
    def log_message(self, format, *args):
        pass

    def _respond(self, status, result):
        body = encode_result(result) if 'type' in result else json.dumps(result).encode('utf-8')
        self.send_response(status)
        self.send_header('Content-Type', 'application/json; charset=utf-8')
        self.send_header('Content-Length', str(len(body)))
        self.send_header('Connection', 'close')
        self.end_headers()
        self.close_connection = True
        self.wfile.write(body)

    def do_POST(self):
        global m5_client_ip
        if self.path != '/audio':
            self._respond(404, {'error': 'Rota desconhecida.'})
            return
        if not is_allowed_audio_peer(self.client_address[0]) or self.headers.get('Origin'):
            self._respond(403, {'error': 'Origem não autorizada. Configure M5_DEVICE_IP.'})
            return
        try:
            if self.headers.get('Transfer-Encoding'):
                raise ValueError('Transfer-Encoding não suportado.')
            length = self.headers.get('Content-Length')
            if length is None or not re.fullmatch(r'[0-9]+', length):
                raise ValueError('Content-Length obrigatório e inválido.')
            size = int(length)
            mode = self.headers.get('X-Voice-Mode', 'ALEXA')
            validate_audio(size, mode)
            content_type = self.headers.get('Content-Type', 'application/octet-stream').split(';')[0].strip()
            if content_type not in ('application/octet-stream', 'audio/pcm'):
                raise ValueError('Content-Type deve ser application/octet-stream.')
        except ValueError as error:
            self._respond(400, {'error': str(error)})
            return
        if not processing_lock.acquire(blocking=False):
            self._respond(503, _result('', 'OCUPADO', 'Outra captura está em processamento.', success=False))
            return
        try:
            self.connection.settimeout(10)
            raw = self.rfile.read(size)
            if len(raw) != size:
                self._respond(400, {'error': 'Áudio incompleto.'})
                return
            m5_client_ip = self.client_address[0]
            result = process_audio_pcm(raw, mode=mode)
            self._respond(200, result)
        except (TimeoutError, socket.timeout):
            self._respond(408, {'error': 'Recepção de áudio expirou.'})
        except (BrokenPipeError, ConnectionResetError):
            pass  # Do not repeat a command after an uncertain delivery.
        except Exception as error:
            self._respond(500, _result('', 'ERRO', str(error)[:150], success=False))
        finally:
            processing_lock.release()

    def do_GET(self):
        if self.path == '/ping':
            self.send_response(200)
            self.send_header('Content-Length', '2')
            self.send_header('Content-Type', 'text/plain')
            self.end_headers()
            self.wfile.write(b'OK')
        elif self.path == '/api/m5/ip':
            self._respond(200, {'ip': configured_device_ip()})
        else:
            self._respond(404, {'error': 'Rota desconhecida.'})


def start_http_server(host, port):
    server = http.server.ThreadingHTTPServer((host, port), AudioHTTPHandler)
    server.daemon_threads = True
    threading.Thread(target=server.serve_forever, daemon=True).start()
    return server


def submit_serial_audio(port, capture):
    request_id, raw, mode = capture
    if not processing_lock.acquire(blocking=False):
        result = _result('', 'OCUPADO', 'Outra captura está em processamento.', success=False)
        if request_id:
            result['id'] = request_id
        write_serial_result(port, result)
        return

    def worker():
        try:
            def progress(title, body):
                packet = {'type': 'PROGRESS', 'title': title, 'body': body}
                if request_id:
                    packet['id'] = request_id
                write_serial_result(port, packet)
            result = process_audio_pcm(raw, mode=mode, on_progress=progress)
            if request_id:
                result['id'] = request_id
            write_serial_result(port, result)
        except Exception as error:
            print(f'[SERIAL] Falha no processamento/entrega: {error}')
        finally:
            processing_lock.release()
    threading.Thread(target=worker, daemon=True).start()


def main():
    global active_port, ser_global
    if sr is None or serial is None:
        raise SystemExit('Instale as dependências: python -m pip install -r requirements.txt')
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, 'reconfigure'):
            stream.reconfigure(encoding='utf-8', line_buffering=True)
    host = os.environ.get('M5_BRIDGE_HOST', '127.0.0.1')
    http_port = int(number_setting('M5_BRIDGE_PORT', 5000, 1, 65535))
    lan_ip = get_lan_ip()
    if host not in ('127.0.0.1', 'localhost') and not configured_device_ip():
        raise SystemExit('Para escutar na LAN, configure M5_DEVICE_IP ou um cache de IP válido.')
    server = start_http_server(host, http_port)
    print(f'[HTTP] Ponte em http://{host}:{http_port}; origem LAN permitida: {configured_device_ip() or "nenhuma"}')
    stop = threading.Event()
    try:
        while not stop.is_set():
            port_name = find_m5_port()
            if not port_name:
                stop.wait(2.5)
                continue
            active_port = port_name
            port = None
            try:
                port = serial.Serial(port_name, baud_rate, timeout=0.1, write_timeout=3)
                with ser_lock:
                    ser_global = port
                receiver = SerialAudioAssembler()
                line_buffer = bytearray()

                def configure():
                    # An empty address clears stale LAN config while USB remains usable.
                    address = lan_ip if host not in ('127.0.0.1', 'localhost') and lan_ip else ''
                    payload = json.dumps({'type': 'CONFIG', 'ip': address, 'port': http_port}, separators=(',', ':')) + '\n'
                    with ser_lock:
                        port.write(payload.encode('utf-8'))
                        port.write(b'{"type":"READY","agent":"M5 Bridge"}\n')
                print(f'[SERIAL] Conectado: {port_name}')
                configure()
                while not stop.is_set():
                    try:
                        receiver.expire()
                    except TimeoutError as error:
                        print(f'[SERIAL] {error}')
                    chunk = port.readline(2049)
                    if not chunk:
                        continue
                    line_buffer.extend(chunk)
                    if len(line_buffer) > 2048:
                        line_buffer.clear()
                        continue
                    if not line_buffer.endswith(b'\n'):
                        continue
                    line = line_buffer.decode('utf-8', errors='replace').strip()
                    line_buffer.clear()
                    if line == 'VOICE_READY':
                        configure()
                    elif line.startswith('VOICE_AUDIO '):
                        parts = line.split()
                        size, mode = int(parts[1]), parts[2] if len(parts) > 2 else 'ALEXA'
                        validate_audio(size, mode)
                        submit_serial_audio(port, (None, read_serial_audio(port, size), mode))
                    elif line.startswith('{'):
                        try:
                            capture = receiver.feed(json.loads(line))
                            if capture:
                                submit_serial_audio(port, capture)
                        except (ValueError, TimeoutError) as error:
                            print(f'[SERIAL] Captura descartada: {error}')
            except (OSError, ValueError) as error:
                print(f'[SERIAL] {error}. Aguardando reconexão.')
                stop.wait(2.5)
            finally:
                with ser_lock:
                    ser_global = None
                    if port:
                        port.close()
    except KeyboardInterrupt:
        stop.set()
    finally:
        server.shutdown()
        server.server_close()


if __name__ == '__main__':
    main()
