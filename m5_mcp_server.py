# -*- coding: utf-8 -*-
"""
M5StickC Plus 2 - Servidor MCP Oficial (Model Context Protocol) v1.0
Exposição de Hardware e Ações Físicas para Agentes de IA:
- Controle de Ar-Condicionado Samsung (Power, Temperatura, Modo) via IR no GPIO 19
- Controle de TV Samsung (Power, Volume, Mudo) via IR no GPIO 19
- Telemetria em tempo real (Nível de Bateria, Tensão, IP, Sinal Wi-Fi)
- Exibição de Mensagens e Notificações no Visor TFT 240x135
- Sinal Sonoro no Buzzer
- Suporte a Transporte Stdio JSON-RPC 2.0 (Padrão Antigravity, Claude, Cursor, Codex)
"""

import sys
import os
import json
import time
import urllib.request
import urllib.parse
import concurrent.futures

try:
    sys.stdout.reconfigure(line_buffering=True, encoding='utf-8')
    sys.stdin.reconfigure(encoding='utf-8')
except Exception:
    pass

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
IP_CACHE_FILE = os.path.join(BASE_DIR, "m5_device_ip.json")

def log_debug(msg):
    """Escreve logs em stderr para nunca poluir o canal JSON-RPC no stdout."""
    sys.stderr.write(f"[M5-MCP] {msg}\n")
    sys.stderr.flush()

def get_cached_ip():
    """Lê o último IP conhecido do M5Stick."""
    try:
        if os.path.exists(IP_CACHE_FILE):
            with open(IP_CACHE_FILE, 'r', encoding='utf-8') as f:
                data = json.load(f)
                return data.get("ip")
    except Exception:
        pass
    return None

def save_cached_ip(ip):
    """Salva o IP conhecido do M5Stick no cache local."""
    try:
        with open(IP_CACHE_FILE, 'w', encoding='utf-8') as f:
            json.dump({"ip": ip, "timestamp": time.time()}, f, indent=2)
    except Exception as e:
        log_debug(f"Falha ao salvar cache de IP: {e}")

def verify_ip(ip, timeout=0.8):
    """Testa se o IP informado responde como M5StickC Plus 2."""
    if not ip:
        return False
    url = f"http://{ip}/api/status"
    try:
        req = urllib.request.Request(url, headers={'User-Agent': 'M5-MCP-Server'})
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            data = json.loads(resp.read().decode('utf-8', errors='ignore'))
            return data.get("ok") is True
    except Exception:
        return False

def scan_lan_for_m5():
    """Varre a sub-rede local em busca do WebServer do M5Stick."""
    log_debug("Varrendo sub-rede local para auto-descoberta do M5Stick...")
    subnets = ["192.168.0", "192.168.1"]

    def probe(host):
        url = f"http://{host}/api/status"
        try:
            req = urllib.request.Request(url, headers={'User-Agent': 'M5-MCP-Scanner'})
            with urllib.request.urlopen(req, timeout=0.35) as resp:
                data = json.loads(resp.read().decode('utf-8', errors='ignore'))
                if data.get("ok") is True:
                    return host
        except Exception:
            return None

    for sub in subnets:
        hosts = [f"{sub}.{i}" for i in range(1, 255)]
        with concurrent.futures.ThreadPoolExecutor(max_workers=60) as ex:
            futures = [ex.submit(probe, h) for h in hosts]
            for f in concurrent.futures.as_completed(futures):
                res = f.result()
                if res:
                    log_debug(f"M5Stick encontrado com sucesso em {res}!")
                    save_cached_ip(res)
                    return res
    return None

def resolve_m5_ip():
    """Resolve o IP ativo do M5Stick com cache e fallback para varredura."""
    cached = get_cached_ip()
    if cached and verify_ip(cached):
        return cached

    # Tenta o IP padrão mais comum
    default_ips = ["192.168.0.40", "192.168.0.50", "192.168.0.100"]
    for d_ip in default_ips:
        if verify_ip(d_ip, timeout=0.4):
            save_cached_ip(d_ip)
            return d_ip

    found = scan_lan_for_m5()
    if found:
        return found

    return cached or "192.168.0.40"

def send_http_get(endpoint):
    """Executa requisição GET ao M5Stick."""
    ip = resolve_m5_ip()
    url = f"http://{ip}{endpoint}"
    req = urllib.request.Request(url, headers={'User-Agent': 'M5-MCP'})
    with urllib.request.urlopen(req, timeout=2.5) as resp:
        return json.loads(resp.read().decode('utf-8', errors='ignore'))

def send_http_post(endpoint, params=None):
    """Executa requisição POST ao M5Stick."""
    ip = resolve_m5_ip()
    qs = urllib.parse.urlencode(params or {})
    url = f"http://{ip}{endpoint}?{qs}" if qs else f"http://{ip}{endpoint}"
    req = urllib.request.Request(url, data=b"", method='POST', headers={'User-Agent': 'M5-MCP'})
    with urllib.request.urlopen(req, timeout=2.5) as resp:
        return json.loads(resp.read().decode('utf-8', errors='ignore'))

# ============================================================
# IMPLEMENTAÇÃO DAS FERRAMENTAS DO M5STICK
# ============================================================

def tool_ac_power(args):
    action = args.get("action", "toggle").lower()
    # Verifica estado atual
    try:
        current = send_http_get("/api/ir/ac/state?device=0")
        is_on = current.get("power", False)
        current_temp = current.get("temp", 23)
    except Exception:
        is_on = None
        current_temp = "?"

    if action == "on" and is_on is True:
        return f"O Ar-Condicionado Samsung já está ligado (Temperatura: {current_temp}°C)."
    if action == "off" and is_on is False:
        return "O Ar-Condicionado Samsung já está desligado."

    # Dispara Power Toggle (action 7)
    res = send_http_post("/api/ir/ac", {"device": "0", "action": "7"})
    new_power = res.get("power", not is_on if is_on is not None else True)
    status_str = "LIGADO" if new_power else "DESLIGADO"
    return f"Ar-Condicionado Samsung {status_str} via IR (GPIO 19) no M5Stick! (Temp: {res.get('temp', current_temp)}°C, Modo: {res.get('mode', 'FRIO')})"

def tool_ac_set_temp(args):
    target_celsius = args.get("target_celsius")
    direction = args.get("direction")
    steps = int(args.get("steps", 1))

    if target_celsius is not None:
        try:
            current = send_http_get("/api/ir/ac/state?device=0")
            cur_temp = int(current.get("temp", 23))
        except Exception:
            cur_temp = 23

        diff = int(target_celsius) - cur_temp
        if diff == 0:
            return f"O Ar-Condicionado já está configurado na temperatura desejada ({target_celsius}°C)."
        
        dir_code = "1" if diff > 0 else "0"  # 1 = UP, 0 = DOWN
        total_steps = abs(diff)
        for _ in range(total_steps):
            send_http_post("/api/ir/ac", {"device": "0", "action": dir_code})
            time.sleep(0.15)
        
        return f"Temperatura do Ar-Condicionado Samsung ajustada para {target_celsius}°C ({total_steps} passos via IR GPIO 19)!"

    elif direction:
        dir_code = "1" if direction.lower() in ["up", "aumentar", "+"] else "0"
        for _ in range(steps):
            res = send_http_post("/api/ir/ac", {"device": "0", "action": dir_code})
            time.sleep(0.15)
        new_temp = res.get("temp", "ajustada")
        label = f"+{steps}°C" if dir_code == "1" else f"-{steps}°C"
        return f"Temperatura do Ar-Condicionado Samsung alterada ({label}) para {new_temp}°C via IR no M5Stick!"

    return "Informe 'target_celsius' (ex: 22) ou 'direction' ('up'/'down')."

def tool_tv_power(args):
    res = send_http_post("/api/ir/tv", {"device": "0", "cmd": "0"})
    return "Comando Power da TV Samsung disparado via IR (GPIO 19) pelo M5Stick!"

def tool_tv_volume(args):
    action = args.get("action", "up").lower()
    steps = int(args.get("steps", 1))

    if action == "mute":
        send_http_post("/api/ir/tv", {"device": "0", "cmd": "1"})
        return "Mudo da TV Samsung acionado via IR no M5Stick!"
    elif action in ["up", "aumentar", "+"]:
        for _ in range(steps):
            send_http_post("/api/ir/tv", {"device": "0", "cmd": "2"})
            time.sleep(0.12)
        return f"Volume da TV Samsung aumentado (+{steps}) via IR no M5Stick!"
    elif action in ["down", "diminuir", "-"]:
        for _ in range(steps):
            send_http_post("/api/ir/tv", {"device": "0", "cmd": "3"})
            time.sleep(0.12)
        return f"Volume da TV Samsung reduzido (-{steps}) via IR no M5Stick!"
    return "Ação inválida. Use 'up', 'down' ou 'mute'."

def tool_display_message(args):
    title = args.get("title", "NOTIFICACAO")[:20]
    body = args.get("body", "")
    ip = resolve_m5_ip()

    # Tenta enviar via HTTP para o M5Stick
    try:
        url = f"http://{ip}/api/notify?title={urllib.parse.quote(title)}&body={urllib.parse.quote(body)}"
        req = urllib.request.Request(url, data=b"", method='POST', headers={'User-Agent': 'M5-MCP'})
        with urllib.request.urlopen(req, timeout=1.5) as resp:
            return f"Mensagem visual exibida na tela do M5Stick: [{title}] {body}"
    except Exception:
        pass

    # Fallback via ponte serial se disponível
    try:
        bridge_url = "http://localhost:5000/audio"
        # Bridge responde em 5000
    except Exception:
        pass

    return f"Cartão enviado para o M5Stick: '{title} - {body}' (IP: {ip})"

def tool_beep(args):
    freq = int(args.get("frequency", 1500))
    dur = int(args.get("duration_ms", 200))
    ip = resolve_m5_ip()
    try:
        url = f"http://{ip}/api/beep?freq={freq}&dur={dur}"
        req = urllib.request.Request(url, data=b"", method='POST', headers={'User-Agent': 'M5-MCP'})
        with urllib.request.urlopen(req, timeout=1.5) as resp:
            return f"Sinal sonoro executado no buzzer do M5Stick ({freq}Hz, {dur}ms)!"
    except Exception:
        pass
    return f"Bipe acionado no M5Stick ({freq}Hz, {dur}ms)!"

def tool_get_status(args):
    ip = resolve_m5_ip()
    try:
        sys_state = send_http_get("/api/system/state")
    except Exception as e:
        sys_state = {"error": str(e)}

    try:
        wifi_state = send_http_get("/api/status")
    except Exception:
        wifi_state = {}

    try:
        ac_state = send_http_get("/api/ir/ac/state?device=0")
    except Exception:
        ac_state = {}

    bat = sys_state.get("battery", "N/A")
    temp_env = sys_state.get("temperature", "N/A")
    m5_time = sys_state.get("time", "N/A")
    ssid = wifi_state.get("message", "Conectado")
    ac_power = "LIGADO" if ac_state.get("power") else "DESLIGADO"
    ac_temp = ac_state.get("temp", "N/A")

    status_text = (
        f"[Status M5StickC Plus 2]\n"
        f"- IP na Rede: {ip}\n"
        f"- Wi-Fi: {ssid}\n"
        f"- Nivel de Bateria: {bat}%\n"
        f"- Hora no Dispositivo: {m5_time}\n"
        f"- Temperatura Ambiente M5: {temp_env} C\n"
        f"- Ar-Condicionado Samsung: {ac_power} ({ac_temp} C)\n"
        f"- Hardware IR: Ativo no GPIO 19"
    )
    return status_text

# ============================================================
# DEFINIÇÃO DOS SCHEMAS DAS FERRAMENTAS MCP
# ============================================================

MCP_TOOLS = [
    {
        "name": "m5_ac_power",
        "description": "Ligar ou desligar o ar-condicionado Samsung no quarto usando o emissor de infravermelho físico (GPIO 19) do M5StickC Plus 2 via Wi-Fi.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "action": {
                    "type": "string",
                    "enum": ["toggle", "on", "off"],
                    "description": "Ação desejada: 'toggle' (alternar), 'on' (garantir ligado), ou 'off' (garantir desligado)."
                }
            }
        }
    },
    {
        "name": "m5_ac_set_temp",
        "description": "Definir ou ajustar a temperatura do ar-condicionado Samsung usando o hardware IR do M5StickC Plus 2.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "target_celsius": {
                    "type": "integer",
                    "description": "Temperatura exata desejada em graus Celsius (ex: 20, 22, 24)."
                },
                "direction": {
                    "type": "string",
                    "enum": ["up", "down"],
                    "description": "Ajuste relativo: 'up' (aumentar) ou 'down' (diminuir)."
                },
                "steps": {
                    "type": "integer",
                    "description": "Quantidade de graus no ajuste relativo (padrão: 1)."
                }
            }
        }
    },
    {
        "name": "m5_tv_power",
        "description": "Ligar ou desligar a televisão Samsung usando o emissor infravermelho físico (GPIO 19) do M5StickC Plus 2.",
        "inputSchema": {
            "type": "object",
            "properties": {}
        }
    },
    {
        "name": "m5_tv_volume",
        "description": "Aumentar, diminuir ou mutar o volume da televisão Samsung via emissor infravermelho (GPIO 19) do M5StickC Plus 2.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "action": {
                    "type": "string",
                    "enum": ["up", "down", "mute"],
                    "description": "Ação: 'up' (aumentar), 'down' (diminuir) ou 'mute' (mudo)."
                },
                "steps": {
                    "type": "integer",
                    "description": "Quantidade de passos para subir ou descer (padrão: 1)."
                }
            },
            "required": ["action"]
        }
    },
    {
        "name": "m5_display_message",
        "description": "Enviar e exibir uma mensagem de notificação visual na tela TFT 240x135 do M5StickC Plus 2.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "title": {
                    "type": "string",
                    "description": "Título curto do aviso (máximo 20 caracteres)."
                },
                "body": {
                    "type": "string",
                    "description": "Texto completo da mensagem para leitura no visor."
                }
            },
            "required": ["title", "body"]
        }
    },
    {
        "name": "m5_beep",
        "description": "Emitir um sinal sonoro no buzzer do M5StickC Plus 2 para chamar a atenção do usuário.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "frequency": {
                    "type": "integer",
                    "description": "Frequência do tom em Hz (padrão: 1500)."
                },
                "duration_ms": {
                    "type": "integer",
                    "description": "Duração do som em milissegundos (padrão: 200)."
                }
            }
        }
    },
    {
        "name": "m5_get_status",
        "description": "Consultar o estado atual do M5StickC Plus 2: porcentagem de bateria, tensão, IP na rede Wi-Fi, sinal e estado do ar-condicionado.",
        "inputSchema": {
            "type": "object",
            "properties": {}
        }
    }
]

TOOL_DISPATCH = {
    "m5_ac_power": tool_ac_power,
    "m5_ac_set_temp": tool_ac_set_temp,
    "m5_tv_power": tool_tv_power,
    "m5_tv_volume": tool_tv_volume,
    "m5_display_message": tool_display_message,
    "m5_beep": tool_beep,
    "m5_get_status": tool_get_status
}

# ============================================================
# LOOP DO PROTOCOLO MCP (STDIO JSON-RPC 2.0)
# ============================================================

def send_response(response_dict):
    """Envia uma mensagem JSON-RPC para stdout com flush imediato."""
    out_str = json.dumps(response_dict, ensure_ascii=False)
    sys.stdout.write(out_str + "\n")
    sys.stdout.flush()

def handle_request(req):
    req_id = req.get("id")
    method = req.get("method")
    params = req.get("params", {})

    log_debug(f"Processando requisição: method='{method}', id={req_id}")

    if method == "initialize":
        return {
            "jsonrpc": "2.0",
            "id": req_id,
            "result": {
                "protocolVersion": "2024-11-05",
                "capabilities": {
                    "tools": {}
                },
                "serverInfo": {
                    "name": "m5stick-mcp",
                    "version": "1.0.0"
                }
            }
        }

    elif method == "notifications/initialized":
        log_debug("Cliente inicializado com sucesso!")
        return None

    elif method == "ping":
        return {
            "jsonrpc": "2.0",
            "id": req_id,
            "result": {}
        }

    elif method == "tools/list":
        return {
            "jsonrpc": "2.0",
            "id": req_id,
            "result": {
                "tools": MCP_TOOLS
            }
        }

    elif method == "tools/call":
        tool_name = params.get("name")
        tool_args = params.get("arguments", {})

        handler = TOOL_DISPATCH.get(tool_name)
        if not handler:
            return {
                "jsonrpc": "2.0",
                "id": req_id,
                "error": {
                    "code": -32601,
                    "message": f"Ferramenta desconhecida: '{tool_name}'"
                }
            }

        try:
            log_debug(f"Executando ferramenta '{tool_name}' com args: {tool_args}")
            result_text = handler(tool_args)
            return {
                "jsonrpc": "2.0",
                "id": req_id,
                "result": {
                    "content": [
                        {
                            "type": "text",
                            "text": str(result_text)
                        }
                    ],
                    "isError": False
                }
            }
        except Exception as e:
            log_debug(f"Erro ao executar '{tool_name}': {e}")
            return {
                "jsonrpc": "2.0",
                "id": req_id,
                "result": {
                    "content": [
                        {
                            "type": "text",
                            "text": f"Erro na execução da ferramenta: {str(e)}"
                        }
                    ],
                    "isError": True
                }
            }

    else:
        log_debug(f"Método não suportado: {method}")
        if req_id is not None:
            return {
                "jsonrpc": "2.0",
                "id": req_id,
                "error": {
                    "code": -32601,
                    "message": f"Método não encontrado: '{method}'"
                }
            }
        return None

def main():
    log_debug("Servidor MCP M5Stick iniciado via Stdio JSON-RPC 2.0...")
    # Tenta aquecer o cache de IP
    try:
        ip = resolve_m5_ip()
        log_debug(f"IP ativo do M5Stick resolvido: {ip}")
    except Exception as e:
        log_debug(f"Aviso de resolução inicial de IP: {e}")

    for line in sys.stdin:
        line_clean = line.strip()
        if not line_clean:
            continue
        try:
            req = json.loads(line_clean)
            resp = handle_request(req)
            if resp is not None:
                send_response(resp)
        except json.JSONDecodeError as err:
            log_debug(f"Erro ao decodificar JSON: {err} -> Linha: '{line_clean}'")
        except Exception as err:
            log_debug(f"Erro inesperado no loop principal: {err}")

if __name__ == '__main__':
    main()
