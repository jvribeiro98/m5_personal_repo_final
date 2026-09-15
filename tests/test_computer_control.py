"""Regression tests: every desktop/browser boundary is replaced before use."""
import ctypes
import importlib.util
from pathlib import Path
import types
import unittest
from unittest.mock import MagicMock, patch, call

ROOT = Path(__file__).resolve().parents[1]


def load_module(name, filename):
    spec = importlib.util.spec_from_file_location(name, ROOT / filename)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class CommandTests(unittest.TestCase):
    def setUp(self):
        self.desktop = MagicMock()
        self.desktop.get_screen_size.return_value = (1920, 1080)
        with patch.dict("sys.modules", {"os_controller": self.desktop}):
            self.agent = load_module("computer_use_under_test", "computer_use_agent.py")
        self.browser = patch.object(self.agent.webbrowser, "open", return_value=True).start()
        self.process = patch.object(self.agent.subprocess, "Popen").start()
        patch.object(self.agent.os.path, "exists", return_value=False).start()
        patch.object(self.agent.time, "sleep").start()
        self.addCleanup(patch.stopall)

    def test_typing_preserves_case_and_does_not_execute_embedded_commands(self):
        result = self.agent.handle_computer_use("Digite Feche o programa AGORA")
        self.desktop.type_text.assert_called_once_with("Feche o programa AGORA")
        self.desktop.hotkey.assert_not_called()
        self.assertEqual(result["type"], "RESULT")

    def test_enter_word_in_payload_is_not_a_submit_command(self):
        self.agent.handle_computer_use("digite Press Enter to continue")
        self.desktop.type_text.assert_called_once_with("Press Enter to continue")
        self.desktop.key_press.assert_not_called()

    def test_explicit_enter_suffix_submits_once(self):
        self.agent.handle_computer_use("digite João Ribeiro e dê enter")
        self.desktop.type_text.assert_called_once_with("João Ribeiro")
        self.desktop.key_press.assert_called_once_with("enter")

    def test_play_pause_is_one_toggle_for_all_supported_verbs(self):
        for command in ("pause o vídeo", "pausar", "retomar", "continuar", "dar play"):
            with self.subTest(command=command):
                self.desktop.reset_mock()
                self.assertIsNotNone(self.agent.handle_computer_use(command))
                self.desktop.media_play_pause.assert_called_once_with()
                self.desktop.key_press.assert_not_called()

    def test_discussion_and_negated_commands_do_not_control_desktop(self):
        for text in ("não feche o programa", "explique como abrir nova aba", "o que significa tela cheia?", "não abra o whatsapp"):
            with self.subTest(text=text):
                self.assertIsNone(self.agent.handle_computer_use(text))
        self.assertEqual(self.desktop.mock_calls, [])
        self.browser.assert_not_called()

    def test_search_payload_does_not_trigger_media_or_window_commands(self):
        self.agent.handle_computer_use("pesquise no google como fechar aba e dar play")
        self.assertEqual(self.desktop.mock_calls, [])
        self.assertIn("google.com/search?", self.browser.call_args.args[0])

    def test_failed_browser_open_returns_an_error_result(self):
        self.browser.return_value = False
        result = self.agent.handle_computer_use("abrir youtube")
        self.assertEqual(result["title"], "ERRO")
        self.assertFalse(result["auto_resume"])
        self.process.assert_not_called()

    def test_browser_failure_never_falls_back_to_a_shell(self):
        self.browser.side_effect = OSError("browser unavailable")
        self.assertFalse(self.agent.open_url("https://example.com/?a=1&b=2"))
        self.process.assert_not_called()

    def test_invalid_url_does_not_launch_anything(self):
        for url in ("javascript:alert(1)", "file:///C:/file", "https://example.com/\ncmd", "https://", None):
            with self.subTest(url=url):
                self.assertFalse(self.agent.open_url(url))
        self.browser.assert_not_called()
        self.process.assert_not_called()

    def test_youtube_search_does_not_click_unverified_coordinates(self):
        result = self.agent.handle_computer_use("pesquise no youtube por AC DC e dê play")
        self.assertIn("youtube.com/results?", self.browser.call_args.args[0])
        self.desktop.mouse_click.assert_not_called()
        self.desktop.key_press.assert_not_called()
        self.assertNotEqual(result["title"], "EM REPRODUÇÃO")

    def test_controller_failure_stays_in_computer_result(self):
        self.desktop.hotkey.side_effect = OSError("desktop unavailable")
        result = self.agent.handle_computer_use("nova aba")
        self.assertEqual(result["type"], "RESULT")
        self.assertEqual(result["title"], "ERRO")
        self.assertFalse(result["auto_resume"])

    def test_screen_metadata_does_not_save_an_unused_screenshot(self):
        self.desktop.get_active_window_title.return_value = "Editor"
        result = self.agent.handle_computer_use("o que está na tela?")
        self.assertIn("Editor", result["body"])
        self.assertIn("1920x1080", result["body"])
        self.desktop.capture_screen.assert_not_called()

    def test_tv_commands_are_left_for_the_ir_dispatcher(self):
        for command in ("aumente o volume da TV", "mute a televisão", "desligue o ar condicionado"):
            self.assertIsNone(self.agent.handle_computer_use(command))
        self.assertEqual(self.desktop.mock_calls, [])

    def test_music_playback_commands_open_youtube_search(self):
        for cmd in ("toque coldplay", "toca queen", "coloque beatles", "bota lofi"):
            with self.subTest(cmd=cmd):
                self.browser.reset_mock()
                result = self.agent.handle_computer_use(cmd)
                self.assertEqual(result["type"], "RESULT")
                self.assertEqual(result["title"], "BUSCA YOUTUBE")
                self.assertTrue(self.browser.called)
                self.assertIn("youtube.com/results?", self.browser.call_args.args[0])

    def test_lock_workstation_command(self):
        for cmd in ("bloquear tela", "bloqueia o pc", "travar tela"):
            with self.subTest(cmd=cmd):
                self.desktop.reset_mock()
                result = self.agent.handle_computer_use(cmd)
                self.assertEqual(result["type"], "RESULT")
                self.assertEqual(result["title"], "PC BLOQUEADO")
                self.desktop.lock_workstation.assert_called_once_with()

    def test_app_launch_commands(self):
        result = self.agent.handle_computer_use("abrir calculadora")
        self.assertEqual(result["type"], "RESULT")
        self.assertEqual(result["title"], "CALCULADORA")
        self.process.assert_called_once_with(["calc.exe"])

    def test_unknown_app_returns_error_and_does_not_pass_through(self):
        result = self.agent.handle_computer_use("abrir aplicativo_inexistente_xyz_99")
        self.assertEqual(result["type"], "RESULT")
        self.assertEqual(result["title"], "ERRO")
        self.assertFalse(result["auto_resume"])


class Win32Tests(unittest.TestCase):
    def setUp(self):
        self.user = MagicMock()
        self.gdi = MagicMock()
        self.kernel = MagicMock()
        for dll in (self.user, self.gdi, self.kernel):
            for name in ("OpenInputDesktop", "SetThreadDesktop", "CloseDesktop", "GetThreadDesktop", "GetCurrentThreadId", "GetCursorPos", "SetCursorPos", "GetDC", "CreateCompatibleDC", "CreateCompatibleBitmap", "SelectObject", "BitBlt", "GetDIBits", "DeleteObject", "DeleteDC", "ReleaseDC"):
                getattr(dll, name).return_value = 1
        self.user.GetSystemMetrics.side_effect = lambda index: {0: 2, 1: 2}[index]
        self.user.GetThreadDesktop.return_value = 12
        self.user.OpenInputDesktop.return_value = 13
        self.gdi.GetDIBits.return_value = 2
        self.user.VkKeyScanW.side_effect = lambda char: ord(char.upper())
        libraries = {"user32": self.user, "gdi32": self.gdi, "kernel32": self.kernel}
        with patch.object(ctypes, "windll", types.SimpleNamespace(**libraries), create=True), patch.object(ctypes, "WinDLL", side_effect=lambda name, **kw: libraries[name], create=True):
            self.controller = load_module("os_controller_under_test", "os_controller.py")
        patch.object(self.controller.time, "sleep").start()
        self.addCleanup(patch.stopall)

    def test_handles_use_pointer_width_in_64_bit_python(self):
        for func in (self.user.OpenInputDesktop, self.user.GetDC, self.gdi.CreateCompatibleDC, self.gdi.CreateCompatibleBitmap, self.gdi.SelectObject):
            self.assertIs(func.restype, ctypes.c_void_p)

    def test_unicode_input_uses_utf16_sendinput(self):
        captured = []
        def send_input(count, inputs, size):
            captured.extend((inputs[i].ki.wVk, inputs[i].ki.wScan, inputs[i].ki.dwFlags) for i in range(count))
            return count
        self.user.SendInput.side_effect = send_input
        self.controller.type_text("á😀")
        self.assertEqual(captured, [(0, 0xE1, 4), (0, 0xE1, 6), (0, 0xD83D, 4), (0, 0xD83D, 6), (0, 0xDE00, 4), (0, 0xDE00, 6)])
        self.user.keybd_event.assert_not_called()

    def test_invalid_hotkey_is_rejected_before_any_key_event(self):
        with self.assertRaises(ValueError):
            self.controller.hotkey("ctrl", "not-a-key")
        self.user.keybd_event.assert_not_called()

    def test_layout_modifier_is_applied_to_character_key(self):
        self.user.VkKeyScanW.side_effect = None
        self.user.VkKeyScanW.return_value = 0x0141  # Shift + A
        self.controller.key_press("A")
        self.assertEqual(self.user.keybd_event.call_args_list,
                         [call(0x10, 0, 0, 0), call(0x41, 0, 0, 0),
                          call(0x41, 0, 2, 0), call(0x10, 0, 2, 0)])

    def test_unmappable_character_does_not_send_virtual_key_255(self):
        self.user.VkKeyScanW.side_effect = None
        self.user.VkKeyScanW.return_value = -1
        with self.assertRaises(ValueError):
            self.controller.key_press("é")
        self.user.keybd_event.assert_not_called()

    def test_sendinput_rejection_is_reported(self):
        self.user.SendInput.return_value = 0
        with self.assertRaises(OSError):
            self.controller.type_text("A")
        self.user.CloseDesktop.assert_called_once_with(13)

    def test_failed_desktop_attach_closes_open_handle(self):
        self.user.SetThreadDesktop.return_value = 0
        with self.assertRaises(OSError):
            self.controller.key_press("enter")
        self.user.CloseDesktop.assert_called_once_with(13)
        self.user.keybd_event.assert_not_called()

    def test_modifier_is_released_when_key_sequence_is_interrupted(self):
        self.controller.time.sleep.side_effect = RuntimeError("interrupted")
        with self.assertRaises(RuntimeError):
            self.controller.hotkey("ctrl", "t")
        self.assertIn(call(0x11, 0, 2, 0), self.user.keybd_event.call_args_list)

    def test_bad_mouse_arguments_fail_before_moving_or_clicking(self):
        for kwargs in ({"button": "unknown"}, {"clicks": -1}, {"x": 5}, {"x": "5", "y": 2}):
            with self.subTest(kwargs=kwargs), self.assertRaises((ValueError, TypeError)):
                self.controller.mouse_click(**kwargs)
        self.user.SetCursorPos.assert_not_called()
        self.user.mouse_event.assert_not_called()

    def test_capture_failure_cleans_resources_and_reports_error(self):
        self.gdi.BitBlt.return_value = 0
        with self.assertRaises(OSError):
            self.controller.capture_screen()
        self.gdi.DeleteObject.assert_called_once_with(1)
        self.gdi.DeleteDC.assert_called_once_with(1)
        self.user.ReleaseDC.assert_called_once_with(0, 1)
        self.assertEqual(self.gdi.SelectObject.call_count, 2)

    def test_capture_deselects_bitmap_and_releases_desktop(self):
        result = self.controller.capture_screen()
        self.assertEqual(result.size, (2, 2))
        self.assertEqual(result.mode, "RGB")
        self.assertEqual(self.gdi.SelectObject.call_count, 2)
        self.user.SetThreadDesktop.assert_has_calls([call(13), call(12)])
        self.user.CloseDesktop.assert_called_once_with(13)

    def test_unreadable_bitmap_is_not_returned_as_success(self):
        self.gdi.GetDIBits.return_value = 0
        with self.assertRaises(OSError):
            self.controller.capture_screen()
        self.gdi.DeleteObject.assert_called_once_with(1)

    def test_failed_bitmap_allocation_releases_existing_dcs(self):
        self.gdi.CreateCompatibleBitmap.return_value = 0
        with self.assertRaises(OSError):
            self.controller.capture_screen()
        self.gdi.DeleteObject.assert_not_called()
        self.gdi.DeleteDC.assert_called_once_with(1)
        self.user.ReleaseDC.assert_called_once_with(0, 1)


if __name__ == "__main__":
    unittest.main()
