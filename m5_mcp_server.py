# -*- coding: utf-8 -*-
"""MCP stdio tools. Configure M5_DEVICE_IP or M5_IP_CACHE_FILE; no serial access."""
import ipaddress
import json
import os
import sys
import time
import urllib.parse
import urllib.request

IP_CACHE_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'm5_device_ip.json')
PROTOCOL_VERSIONS = ('2024-11-05', '2025-03-26', '2025-06-18')


def log_debug(message):
    print(f'[M5-MCP] {message}', file=sys.stderr, flush=True)


def _device_ip(value):
    if not isinstance(value, str):
        raise ValueError('M5_DEVICE_IP deve ser um endereço IPv4.')
    address = ipaddress.IPv4Address(value.strip())
    if address.is_unspecified or address.is_multicast or str(address) == '255.255.255.255':
        raise ValueError('M5_DEVICE_IP deve identificar um dispositivo.')
    return str(address)


def get_cached_ip():
    try:
        with open(os.environ.get('M5_IP_CACHE_FILE', IP_CACHE_FILE), encoding='utf-8') as source:
            return _device_ip(json.load(source).get('ip'))
    except (OSError, ValueError, TypeError, AttributeError):
        return None


def resolve_m5_ip():
    configured = os.environ.get('M5_DEVICE_IP', '').strip()
    if configured:
        return _device_ip(configured)
    cached = get_cached_ip()
    if cached:
        return cached
    raise RuntimeError('IP do M5 desconhecido. Configure M5_DEVICE_IP ou M5_IP_CACHE_FILE.')


def _http_request(endpoint, params=None, method='GET'):
    ip = resolve_m5_ip()
    timeout = float(os.environ.get('M5_HTTP_TIMEOUT', '5'))
    if not 0 < timeout <= 30:
        raise ValueError('M5_HTTP_TIMEOUT deve estar entre 0 e 30 segundos.')
    query = urllib.parse.urlencode(params or {})
    url = f'http://{ip}{endpoint}' + (('?' + query) if query else '')
    request = urllib.request.Request(url, data=b'' if method == 'POST' else None,
                                     method=method, headers={'User-Agent': 'M5-MCP'})
    # Never retry a POST: an absent response does not mean no IR was emitted.
    with urllib.request.urlopen(request, timeout=timeout) as response:
        raw = response.read(65537)
    if len(raw) > 65536:
        raise RuntimeError('Resposta do M5 excedeu o limite de tamanho.')
    data = json.loads(raw.decode('utf-8'))
    if not isinstance(data, dict) or data.get('ok') is not True:
        message = data.get('message', 'Resposta inválida do M5') if isinstance(data, dict) else 'Resposta inválida do M5'
        raise RuntimeError(str(message))
    return data


def send_http_get(endpoint):
    return _http_request(endpoint)


def send_http_post(endpoint, params=None):
    return _http_request(endpoint, params, 'POST')


def tool_ac_power(args):
    action = args.get('action', 'toggle')
    params = {'device': '0'}
    params.update({'action': '7'} if action == 'toggle' else {'power': action})
    result = send_http_post('/api/ir/ac', params)
    if type(result.get('power')) is not bool:
        raise RuntimeError('O M5 não informou o estado de energia após o comando.')
    state = 'LIGADO' if result['power'] else 'DESLIGADO'
    return f'Comando IR do ar enviado. Estado configurado: {state} ({result.get("temp", "?")}°C).'


def tool_ac_set_temp(args):
    if 'target_celsius' in args:
        target = args['target_celsius']
        result = send_http_post('/api/ir/ac', {'device': '0', 'temp': str(target)})
        if result.get('temp') != target:
            raise RuntimeError('O M5 não confirmou a temperatura solicitada.')
    else:
        action = '1' if args['direction'] == 'up' else '0'
        steps = args.get('steps', 1)
        for index in range(steps):
            result = send_http_post('/api/ir/ac', {'device': '0', 'action': action})
            if index + 1 < steps:
                time.sleep(0.15)
    return f'Comando IR enviado. Temperatura configurada no M5: {result.get("temp", "?")}°C.'


def tool_tv_power(args):
    send_http_post('/api/ir/tv', {'device': '0', 'cmd': '0'})
    return 'Comando de alternar energia da TV Samsung enviado pelo M5.'


def tool_tv_volume(args):
    action = args['action']
    command = {'mute': '1', 'up': '2', 'down': '3'}[action]
    steps = 1 if action == 'mute' else args.get('steps', 1)
    for index in range(steps):
        send_http_post('/api/ir/tv', {'device': '0', 'cmd': command})
        if index + 1 < steps:
            time.sleep(0.12)
    return f'Comando de volume da TV enviado: {action}, {steps} passo(s).'


def tool_display_message(args):
    send_http_post('/api/notify', {'title': args['title'], 'body': args['body']})
    return f'Mensagem aceita pelo M5: [{args["title"]}] {args["body"]}'


def tool_beep(args):
    frequency, duration = args.get('frequency', 1500), args.get('duration_ms', 200)
    send_http_post('/api/beep', {'freq': frequency, 'dur': duration})
    return f'Sinal sonoro aceito pelo M5 ({frequency}Hz, {duration}ms).'


def tool_get_status(args):
    ip = resolve_m5_ip()
    system = send_http_get('/api/system/state')
    wifi = send_http_get('/api/status')
    ac = send_http_get('/api/ir/ac/state?device=0')
    power = {True: 'LIGADO', False: 'DESLIGADO'}.get(ac.get('power'), 'DESCONHECIDO')
    temperature = system.get('temperature')
    return (f'[M5StickC Plus 2]\nIP: {ip}\nWi-Fi: {wifi.get("message", "N/A")}\n'
            f'Bateria: {system.get("battery", "N/A")}%\nHora: {system.get("time", "N/A")}\n'
            f'Temperatura externa (clima): {temperature if temperature is not None else "N/A"}°C\n'
            f'Ar Samsung, estado configurado: {power} ({ac.get("temp", "N/A")}°C)')


def _tool(name, description, properties, required=()):
    schema = {'type': 'object', 'properties': properties, 'additionalProperties': False}
    if required:
        schema['required'] = list(required)
    return {'name': name, 'description': description, 'inputSchema': schema}


MCP_TOOLS = [
    _tool('m5_ac_power', 'Configurar energia do ar Samsung via IR. Estado estimado pelo M5, sem confirmação física.', {
        'action': {'type': 'string', 'enum': ['toggle', 'on', 'off'], 'default': 'toggle'}}),
    _tool('m5_ac_set_temp', 'Configurar temperatura do ar. Informe target_celsius OU direction; steps é relativo.', {
        'target_celsius': {'type': 'integer', 'minimum': 17, 'maximum': 30},
        'direction': {'type': 'string', 'enum': ['up', 'down']},
        'steps': {'type': 'integer', 'minimum': 1, 'maximum': 13, 'default': 1}}),
    _tool('m5_tv_power', 'Alternar energia da TV Samsung. Repetir inverte a energia novamente.', {}),
    _tool('m5_tv_volume', 'Ajustar volume da TV Samsung via IR.', {
        'action': {'type': 'string', 'enum': ['up', 'down', 'mute']},
        'steps': {'type': 'integer', 'minimum': 1, 'maximum': 20, 'default': 1}}, ['action']),
    _tool('m5_display_message', 'Exibir uma notificação no M5.', {
        'title': {'type': 'string', 'minLength': 1, 'maxLength': 20},
        'body': {'type': 'string', 'maxLength': 500}}, ['title', 'body']),
    _tool('m5_beep', 'Emitir sinal sonoro no M5.', {
        'frequency': {'type': 'integer', 'minimum': 20, 'maximum': 20000, 'default': 1500},
        'duration_ms': {'type': 'integer', 'minimum': 1, 'maximum': 5000, 'default': 200}}),
    _tool('m5_get_status', 'Consultar bateria, Wi-Fi, horário, clima e estado configurado do ar.', {}),
]
MCP_TOOLS[1]['inputSchema']['oneOf'] = [
    {'required': ['target_celsius'], 'not': {'anyOf': [{'required': ['direction']}, {'required': ['steps']}]}},
    {'required': ['direction'], 'not': {'required': ['target_celsius']}},
]
TOOL_DISPATCH = {
    'm5_ac_power': tool_ac_power, 'm5_ac_set_temp': tool_ac_set_temp,
    'm5_tv_power': tool_tv_power, 'm5_tv_volume': tool_tv_volume,
    'm5_display_message': tool_display_message, 'm5_beep': tool_beep,
    'm5_get_status': tool_get_status,
}


def validate_tool_args(name, args):
    if not isinstance(args, dict):
        raise ValueError('arguments deve ser um objeto JSON.')
    schema = next(tool['inputSchema'] for tool in MCP_TOOLS if tool['name'] == name)
    properties = schema['properties']
    if set(args) - set(properties):
        raise ValueError('Argumento desconhecido.')
    for required in schema.get('required', []):
        if required not in args:
            raise ValueError(f'Argumento obrigatório: {required}.')
    for key, value in args.items():
        prop = properties[key]
        if prop['type'] == 'integer':
            if type(value) is not int or not prop['minimum'] <= value <= prop['maximum']:
                raise ValueError(f'{key} deve ser inteiro entre {prop["minimum"]} e {prop["maximum"]}.')
        elif not isinstance(value, str):
            raise ValueError(f'{key} deve ser texto.')
        elif len(value) < prop.get('minLength', 0) or len(value) > prop.get('maxLength', 10000):
            raise ValueError(f'Tamanho inválido para {key}.')
        if 'enum' in prop and value not in prop['enum']:
            raise ValueError(f'Valor inválido para {key}.')
    if name == 'm5_ac_set_temp':
        absolute, relative = 'target_celsius' in args, 'direction' in args
        if absolute == relative or (absolute and 'steps' in args):
            raise ValueError('Informe target_celsius OU direction (com steps opcional).')


def _error(req_id, code, message):
    return {'jsonrpc': '2.0', 'id': req_id, 'error': {'code': code, 'message': message}}


def _result(req_id, result):
    return {'jsonrpc': '2.0', 'id': req_id, 'result': result}


def handle_request(req):
    if not isinstance(req, dict) or req.get('jsonrpc') != '2.0' or not isinstance(req.get('method'), str):
        return _error(None, -32600, 'Invalid Request')
    if 'id' not in req:
        return None
    req_id = req['id']
    if type(req_id) not in (str, int) or (isinstance(req_id, str) and not req_id):
        return _error(None, -32600, 'Invalid request ID')
    method, params = req['method'], req.get('params', {})
    if not isinstance(params, dict):
        return _error(req_id, -32602, 'params deve ser um objeto JSON.')
    if method == 'initialize':
        requested = params.get('protocolVersion')
        version = requested if requested in PROTOCOL_VERSIONS else PROTOCOL_VERSIONS[0]
        return _result(req_id, {'protocolVersion': version, 'capabilities': {'tools': {}},
                               'serverInfo': {'name': 'm5stick-mcp', 'version': '1.1.0'}})
    if method == 'ping':
        return _result(req_id, {})
    if method == 'tools/list':
        return _result(req_id, {'tools': MCP_TOOLS})
    if method == 'tools/call':
        name = params.get('name')
        if not isinstance(name, str) or name not in TOOL_DISPATCH:
            return _error(req_id, -32602, 'Ferramenta desconhecida.')
        args = params.get('arguments', {})
        try:
            validate_tool_args(name, args)
        except ValueError as error:
            return _error(req_id, -32602, str(error))
        try:
            text, failed = TOOL_DISPATCH[name](args), False
        except Exception as error:
            log_debug(f'Falha em {name}: {error}')
            text, failed = f'Falha na ferramenta: {error}', True
        return _result(req_id, {'content': [{'type': 'text', 'text': str(text)}], 'isError': failed})
    return _error(req_id, -32601, f'Método desconhecido: {method}')


def send_response(response_dict):
    sys.stdout.write(json.dumps(response_dict, ensure_ascii=False, separators=(',', ':')) + '\n')
    sys.stdout.flush()


def main():
    for stream in (sys.stdin, sys.stdout):
        if hasattr(stream, 'reconfigure'):
            stream.reconfigure(encoding='utf-8')
    log_debug('Servidor MCP iniciado em stdio; hardware consultado sob demanda.')
    for line in sys.stdin:
        if not line.strip():
            continue
        try:
            request = json.loads(line)
        except (json.JSONDecodeError, ValueError):
            send_response(_error(None, -32700, 'Parse error'))
            continue
        try:
            response = handle_request(request)
        except Exception as error:
            log_debug(f'Erro inesperado: {error}')
            response = _error(request.get('id') if isinstance(request, dict) else None, -32603, 'Internal error')
        if response is not None:
            send_response(response)


if __name__ == '__main__':
    main()
