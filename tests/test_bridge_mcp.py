"""Offline contract regressions: no serial port, HTTP listener or desktop actions."""
import contextlib
import base64
import importlib.util
import io
import json
from pathlib import Path
import struct
import sys
import types
import unittest
from unittest.mock import Mock, patch
import numpy  # Keep the actual DSP dependency loaded outside the import stubs.

ROOT = Path(__file__).resolve().parents[1]


def load(name):
    spec = importlib.util.spec_from_file_location(name, ROOT / (name + '.py'))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


serial_stub = types.ModuleType('serial')
serial_stub.tools = types.ModuleType('serial.tools')
serial_stub.tools.list_ports = types.ModuleType('serial.tools.list_ports')
serial_stub.tools.list_ports.comports = lambda: []
serial_stub.SerialException = OSError
speech_stub = types.ModuleType('speech_recognition')
speech_stub.Recognizer = Mock
speech_stub.AudioData = lambda data, rate, width: types.SimpleNamespace(frame_data=data)
speech_stub.UnknownValueError = type('UnknownValueError', (Exception,), {})
with patch.dict(sys.modules, {
    'serial': serial_stub, 'serial.tools': serial_stub.tools,
    'serial.tools.list_ports': serial_stub.tools.list_ports,
    'speech_recognition': speech_stub,
    'computer_use_agent': types.SimpleNamespace(handle_computer_use=lambda *a, **kw: None),
}), contextlib.redirect_stdout(io.StringIO()):
    bridge = load('m5_agent_bridge')
    mcp = load('m5_mcp_server')


def call_tool(name, args):
    return mcp.handle_request({'jsonrpc': '2.0', 'id': 7, 'method': 'tools/call',
                               'params': {'name': name, 'arguments': args}})


class BridgeTests(unittest.TestCase):
    def setUp(self):
        self.output = contextlib.redirect_stdout(io.StringIO())
        self.output.__enter__()
        self.addCleanup(self.output.__exit__, None, None, None)

    def test_ir_is_delivered_as_one_tag_without_http_side_effect(self):
        with patch.object(bridge, 'trigger_m5_ir') as http:
            result = bridge.parse_voice_command('desligue o ar condicionado')
        self.assertEqual(result['ir'], 'AC_OFF')
        http.assert_not_called()

    def test_air_question_does_not_toggle_power(self):
        with patch.object(bridge, 'trigger_m5_ir') as http, patch.object(
                bridge, 'query_agy_agent', return_value=('Temperatura externa.', None)):
            result = bridge.parse_voice_command('qual a temperatura do ar hoje?')
        self.assertNotIn('ir', result)
        http.assert_not_called()

    def test_unsupported_imperative_command_does_not_query_agy(self):
        with patch.object(bridge, 'query_agy_agent') as agy:
            result = bridge.parse_voice_command('execute um comando maluco')
        self.assertEqual(result['title'], 'COMANDO NÃO SUPORTADO')
        self.assertFalse(result['auto_resume'])
        agy.assert_not_called()

    def test_processing_http_audio_does_not_send_serial_copy(self):
        port = Mock(is_open=True)
        with patch.object(bridge, 'ser_global', port), patch.object(
                bridge.recognizer, 'recognize_google', return_value='ei m5 desligue a tv'):
            result = bridge.process_audio_pcm(b'\x20\x00' * 400)
        self.assertEqual(result['ir'], 'TV_POWER')
        port.write.assert_not_called()

    def test_negative_full_scale_sample_is_not_mistaken_for_small_peak(self):
        pcm = struct.pack('<hh', -32768, 10000) * 200
        with patch.object(bridge.recognizer, 'recognize_google', return_value='conversa') as recognize:
            bridge.process_audio_pcm(pcm)
        self.assertEqual(recognize.call_args.args[0].frame_data, pcm)

    def test_invalid_pcm_and_mode_are_rejected_before_recognition(self):
        for data, mode in [(b'a' * 801, 'ALEXA'), (b'a' * 800, 'ptt-typo')]:
            with self.subTest(mode=mode), self.assertRaises(ValueError):
                bridge.process_audio_pcm(data, mode=mode)

    def test_wake_word_does_not_accept_prefix_of_another_word(self):
        self.assertEqual(bridge.check_and_strip_wake_word('Alexandre abre a janela')[0], False)
        self.assertEqual(bridge.check_and_strip_wake_word('m50 abre a janela')[0], False)

    def test_timeout_does_not_claim_killed_process_is_running(self):
        with patch.object(bridge.subprocess, 'run', side_effect=bridge.subprocess.TimeoutExpired('agy', 20)):
            reply, ir = bridge.query_agy_agent('teste')
        self.assertNotIn('segundo plano', reply)
        self.assertIsNone(ir)

    def test_failed_cli_cannot_return_an_ir_action(self):
        with patch.object(bridge.subprocess, 'run', return_value=types.SimpleNamespace(
                returncode=1, stdout='[IR:TV_POWER] feito', stderr='failure')):
            reply, ir = bridge.query_agy_agent('teste')
        self.assertIsNone(ir)
        self.assertIn('Falha', reply)

    def test_cli_keeps_its_permission_checks_by_default(self):
        with patch.object(bridge.subprocess, 'run', return_value=types.SimpleNamespace(
                returncode=0, stdout='ok', stderr='')) as run:
            bridge.query_agy_agent('teste')
        self.assertNotIn('--dangerously-skip-permissions', run.call_args.args[0])

    def test_serial_reads_until_complete_instead_of_one_timeout_window(self):
        port = Mock()
        port.read.side_effect = [b'12', b'', b'3456', b'78']
        self.assertEqual(bridge.read_serial_audio(port, 8), b'12345678')

    def test_incomplete_serial_audio_is_not_processed(self):
        port = Mock()
        port.read.return_value = b''
        with patch.object(bridge.time, 'monotonic', side_effect=[0, 0, 999]), self.assertRaises(TimeoutError):
            bridge.read_serial_audio(port, 800)

    def test_unknown_serial_device_does_not_default_to_com3(self):
        with patch.dict(bridge.os.environ, {}, clear=True):
            self.assertIsNone(bridge.find_m5_port())

    def test_framed_audio_requires_matching_id_sequence_and_exact_size(self):
        receiver = bridge.SerialAudioAssembler()
        receiver.feed({'type': 'AUDIO_START', 'id': '42', 'bytes': 4, 'mode': 'PTT'})
        receiver.feed({'type': 'AUDIO_CHUNK', 'id': '42', 'seq': 0, 'data': 'AQIDBA=='})
        self.assertEqual(receiver.feed({'type': 'AUDIO_END', 'id': '42'}),
                         ('42', b'\x01\x02\x03\x04', 'PTT'))
        self.assertIsNone(receiver.feed({'type': 'AUDIO_END', 'id': '42'}))

    def test_out_of_order_truncated_and_expired_frames_are_discarded(self):
        for kind in ['sequence', 'truncated', 'expired']:
            receiver = bridge.SerialAudioAssembler()
            with self.subTest(kind=kind), patch.object(bridge.time, 'monotonic', return_value=0):
                receiver.feed({'type': 'AUDIO_START', 'id': '42', 'bytes': 4, 'mode': 'PTT'})
                with self.assertRaises((ValueError, TimeoutError)):
                    if kind == 'sequence':
                        receiver.feed({'type': 'AUDIO_CHUNK', 'id': '42', 'seq': 1, 'data': 'AQIDBA=='})
                    elif kind == 'truncated':
                        receiver.feed({'type': 'AUDIO_END', 'id': '42'})
                    else:
                        with patch.object(bridge.time, 'monotonic', return_value=11):
                            receiver.feed({'type': 'AUDIO_END', 'id': '42'})
                self.assertIsNone(receiver.feed({'type': 'AUDIO_END', 'id': '42'}))

    def test_audio_peer_must_be_explicitly_configured_for_lan(self):
        with patch.object(bridge, 'configured_device_ip', return_value='192.168.0.40'):
            self.assertTrue(bridge.is_allowed_audio_peer('192.168.0.40'))
            self.assertFalse(bridge.is_allowed_audio_peer('192.168.0.41'))
            self.assertTrue(bridge.is_allowed_audio_peer('127.0.0.1'))

    def test_display_packet_fits_device_response_budget(self):
        raw = bridge.encode_result({'type': 'RESULT', 'body': '\x00' * 10000,
                                    'title': 'á' * 100, 'text': '😀' * 10000})
        self.assertLessEqual(len(raw), 8192)
        self.assertEqual(json.loads(raw)['type'], 'RESULT')

    def test_http_rejects_bad_lengths_without_processing_or_cache_updates(self):
        for length in ['-1', 'invalid', '801', '960002']:
            handler = bridge.AudioHTTPHandler.__new__(bridge.AudioHTTPHandler)
            handler.path = '/audio'
            handler.client_address = ('127.0.0.1', 1234)
            handler.headers = {'Content-Length': length}
            handler.rfile = io.BytesIO(b'0' * 1000)
            handler.wfile = io.BytesIO()
            handler.connection = Mock()
            handler.send_response = Mock()
            handler.send_header = Mock()
            handler.end_headers = Mock()
            with self.subTest(length=length), patch.object(bridge, 'process_audio_pcm', return_value={}) as process, patch('builtins.open', side_effect=OSError('No cache writes in test')):
                handler.do_POST()
                process.assert_not_called()
                self.assertGreaterEqual(handler.send_response.call_args.args[0], 400)


class MCPTests(unittest.TestCase):
    def setUp(self):
        self.logs = contextlib.redirect_stderr(io.StringIO())
        self.logs.__enter__()
        self.addCleanup(self.logs.__exit__, None, None, None)

    def test_initialize_does_not_need_network_discovery(self):
        source = io.StringIO('{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}\n')
        dest = io.StringIO()
        with patch.object(sys, 'stdin', source), patch.object(sys, 'stdout', dest), patch.object(
                mcp, 'resolve_m5_ip', side_effect=AssertionError('No network during handshake')) as resolve:
            mcp.main()
        self.assertEqual(json.loads(dest.getvalue())['id'], 1)
        resolve.assert_not_called()

    def test_configured_ip_skips_discovery_and_rejects_url_injection(self):
        with patch.dict(mcp.os.environ, {'M5_DEVICE_IP': '10.0.0.8'}), patch.object(
                mcp.urllib.request, 'urlopen', side_effect=AssertionError('No discovery')):
            self.assertEqual(mcp.resolve_m5_ip(), '10.0.0.8')
        with patch.dict(mcp.os.environ, {'M5_DEVICE_IP': 'localhost@evil.test/path'}):
            with self.assertRaises(ValueError):
                mcp.resolve_m5_ip()

    def test_unknown_ip_does_not_scan_or_guess_a_device(self):
        with patch.dict(mcp.os.environ, {}, clear=True), patch.object(mcp, 'get_cached_ip', return_value=None), patch.object(
                mcp.urllib.request, 'urlopen', side_effect=AssertionError('No implicit scan')):
            with self.assertRaises(RuntimeError):
                mcp.resolve_m5_ip()

    def test_parse_error_is_reported_to_client_and_loop_continues(self):
        source = io.StringIO('broken\n{"jsonrpc":"2.0","id":2,"method":"ping"}\n')
        dest = io.StringIO()
        with patch.object(sys, 'stdin', source), patch.object(sys, 'stdout', dest), patch.object(
                mcp, 'resolve_m5_ip', return_value='192.168.0.40'):
            mcp.main()
        responses = [json.loads(line) for line in dest.getvalue().splitlines()]
        self.assertEqual(responses[0]['error']['code'], -32700)
        self.assertEqual(responses[1]['id'], 2)

    def test_notifications_do_not_execute_tools_or_get_responses(self):
        req = {'jsonrpc': '2.0', 'method': 'tools/call', 'params': {'name': 'm5_tv_power'}}
        with patch.object(mcp, 'send_http_post') as post:
            response = mcp.handle_request(req)
        self.assertIsNone(response)
        post.assert_not_called()

    def test_invalid_request_and_params_return_json_rpc_errors(self):
        for req, code in [([], -32600), ({'id': 1, 'method': 'ping'}, -32600),
                          ({'jsonrpc': '2.0', 'id': 1, 'method': 'tools/call', 'params': []}, -32602)]:
            with self.subTest(request=req):
                self.assertEqual(mcp.handle_request(req)['error']['code'], code)

    def test_bad_tool_arguments_never_send_hardware_commands(self):
        cases = [('m5_ac_power', {'action': 'invalid'}),
                 ('m5_ac_set_temp', {'direction': 'down', 'steps': 0}),
                 ('m5_ac_set_temp', {'target_celsius': 500}),
                 ('m5_ac_set_temp', {'target_celsius': 22, 'direction': 'up'}),
                 ('m5_tv_volume', {'action': 'up', 'steps': 100000}),
                 ('m5_tv_volume', {'action': 'up', 'steps': True}),
                 ('m5_beep', {'duration_ms': -1}),
                 ('m5_display_message', {'title': 'x', 'body': ['wrong']})]
        for name, args in cases:
            with self.subTest(name=name, args=args), patch.object(mcp, 'send_http_post') as post, patch.object(
                    mcp, 'send_http_get', return_value={'ok': True, 'power': True, 'temp': 23}), patch.object(
                    mcp, 'resolve_m5_ip', return_value='192.168.0.40'), patch.object(mcp.time, 'sleep'), patch.object(
                    mcp.urllib.request, 'urlopen', side_effect=AssertionError('Unexpected network call')):
                result = call_tool(name, args)
                self.assertTrue('error' in result or result['result']['isError'])
                post.assert_not_called()

    def test_failed_notify_and_beep_are_errors_not_success_messages(self):
        for name, args in [('m5_beep', {}), ('m5_display_message', {'title': 'Teste', 'body': 'Olá'})]:
            with self.subTest(name=name), patch.object(mcp, 'resolve_m5_ip', return_value='192.168.0.40'), patch.object(
                    mcp.urllib.request, 'urlopen', side_effect=OSError('offline')):
                self.assertTrue(call_tool(name, args)['result']['isError'])

    def test_device_application_error_is_not_accepted_as_success(self):
        response = Mock()
        response.__enter__ = Mock(return_value=response)
        response.__exit__ = Mock(return_value=False)
        response.read.return_value = b'{"ok":false,"message":"Codigo IR pendente"}'
        with patch.object(mcp, 'resolve_m5_ip', return_value='192.168.0.40'), patch.object(
                mcp.urllib.request, 'urlopen', return_value=response), self.assertRaises(RuntimeError):
            mcp.send_http_post('/api/ir/tv', {'device': '0', 'cmd': '0'})

    def test_absolute_temperature_uses_idempotent_device_setter(self):
        with patch.object(mcp, 'send_http_get', side_effect=OSError('offline')), patch.object(
                mcp, 'send_http_post', return_value={'ok': True, 'temp': 22}) as post, patch.object(mcp.time, 'sleep'):
            response = call_tool('m5_ac_set_temp', {'target_celsius': 22})
        self.assertFalse(response['result']['isError'])
        post.assert_called_once_with('/api/ir/ac', {'device': '0', 'temp': '22'})


if __name__ == '__main__':
    unittest.main()
