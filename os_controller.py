# -*- coding: utf-8 -*-
"""Controle Win32 com tipos de ponteiro, entrada Unicode e recursos gerenciados."""

from contextlib import contextmanager
import ctypes
import math
import time

from PIL import Image

# Os tamanhos Windows são fixos, mesmo ao importar este módulo em outro SO.
DWORD = ctypes.c_uint32
WORD = ctypes.c_uint16
LONG = ctypes.c_int32
HANDLE = ctypes.c_void_p
ULONG_PTR = ctypes.c_size_t
BOOL = ctypes.c_int

MOUSEEVENTF_MOVE = 0x0001
MOUSEEVENTF_LEFTDOWN = 0x0002
MOUSEEVENTF_LEFTUP = 0x0004
MOUSEEVENTF_RIGHTDOWN = 0x0008
MOUSEEVENTF_RIGHTUP = 0x0010
MOUSEEVENTF_MIDDLEDOWN = 0x0020
MOUSEEVENTF_MIDDLEUP = 0x0040
MOUSEEVENTF_WHEEL = 0x0800
MOUSEEVENTF_ABSOLUTE = 0x8000
KEYEVENTF_EXTENDEDKEY = 0x0001
KEYEVENTF_KEYUP = 0x0002
KEYEVENTF_UNICODE = 0x0004

VK_MAPPING = {
    'enter': 0x0D, 'esc': 0x1B, 'escape': 0x1B, 'tab': 0x09,
    'space': 0x20, 'espaço': 0x20, 'backspace': 0x08,
    'delete': 0x2E, 'del': 0x2E, 'up': 0x26, 'down': 0x28,
    'left': 0x25, 'right': 0x27, 'home': 0x24, 'end': 0x23,
    'pageup': 0x21, 'pagedown': 0x22, 'ctrl': 0x11, 'control': 0x11,
    'alt': 0x12, 'shift': 0x10, 'win': 0x5B, 'windows': 0x5B,
    'vol_up': 0xAF, 'vol_down': 0xAE, 'vol_mute': 0xAD,
    'media_play_pause': 0xB3, 'browser_back': 0xA6,
    'browser_forward': 0xA7, 'browser_refresh': 0xA8,
    **{f'f{i}': 0x6F + i for i in range(1, 25)},
}
_EXTENDED_KEYS = {0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
                  0x2D, 0x2E, 0x5B, 0xA6, 0xA7, 0xA8, 0xAD, 0xAE, 0xAF, 0xB3}


class POINT(ctypes.Structure):
    _fields_ = [('x', LONG), ('y', LONG)]


class KEYBDINPUT(ctypes.Structure):
    _fields_ = [('wVk', WORD), ('wScan', WORD), ('dwFlags', DWORD),
                ('time', DWORD), ('dwExtraInfo', ULONG_PTR)]


class MOUSEINPUT(ctypes.Structure):
    _fields_ = [('dx', LONG), ('dy', LONG), ('mouseData', DWORD),
                ('dwFlags', DWORD), ('time', DWORD), ('dwExtraInfo', ULONG_PTR)]


class HARDWAREINPUT(ctypes.Structure):
    _fields_ = [('uMsg', DWORD), ('wParamL', WORD), ('wParamH', WORD)]


class _INPUTUNION(ctypes.Union):
    _fields_ = [('ki', KEYBDINPUT), ('mi', MOUSEINPUT), ('hi', HARDWAREINPUT)]


class INPUT(ctypes.Structure):
    _anonymous_ = ('data',)
    _fields_ = [('type', DWORD), ('data', _INPUTUNION)]


class BITMAPINFOHEADER(ctypes.Structure):
    _fields_ = [
        ('biSize', DWORD), ('biWidth', LONG), ('biHeight', LONG),
        ('biPlanes', WORD), ('biBitCount', WORD), ('biCompression', DWORD),
        ('biSizeImage', DWORD), ('biXPelsPerMeter', LONG),
        ('biYPelsPerMeter', LONG), ('biClrUsed', DWORD), ('biClrImportant', DWORD),
    ]


def _signature(library, name, restype, *argtypes):
    function = getattr(library, name)
    function.restype = restype
    function.argtypes = list(argtypes)


try:
    user32 = ctypes.WinDLL("user32", use_last_error=True)
    gdi32 = ctypes.WinDLL("gdi32", use_last_error=True)
    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
except (AttributeError, OSError):
    user32 = gdi32 = kernel32 = None

if user32 is not None:
    _signature(user32, "OpenInputDesktop", HANDLE, DWORD, BOOL, DWORD)
    _signature(user32, "GetThreadDesktop", HANDLE, DWORD)
    _signature(user32, "SetThreadDesktop", BOOL, HANDLE)
    _signature(user32, "CloseDesktop", BOOL, HANDLE)
    _signature(kernel32, "GetCurrentThreadId", DWORD)
    _signature(user32, "GetSystemMetrics", ctypes.c_int, ctypes.c_int)
    _signature(user32, "GetCursorPos", BOOL, ctypes.POINTER(POINT))
    _signature(user32, "SetCursorPos", BOOL, ctypes.c_int, ctypes.c_int)
    _signature(user32, "mouse_event", None, DWORD, DWORD, DWORD, DWORD, ULONG_PTR)
    _signature(user32, "keybd_event", None, ctypes.c_ubyte, ctypes.c_ubyte, DWORD, ULONG_PTR)
    _signature(user32, "VkKeyScanW", ctypes.c_short, ctypes.c_wchar)
    _signature(user32, "SendInput", ctypes.c_uint, ctypes.c_uint, ctypes.POINTER(INPUT), ctypes.c_int)
    _signature(user32, "GetForegroundWindow", HANDLE)
    _signature(user32, "GetWindowTextLengthW", ctypes.c_int, HANDLE)
    _signature(user32, "GetWindowTextW", ctypes.c_int, HANDLE, ctypes.c_wchar_p, ctypes.c_int)
    _signature(user32, "GetDC", HANDLE, HANDLE)
    _signature(user32, "ReleaseDC", ctypes.c_int, HANDLE, HANDLE)
    _signature(gdi32, "CreateCompatibleDC", HANDLE, HANDLE)
    _signature(gdi32, "CreateCompatibleBitmap", HANDLE, HANDLE, ctypes.c_int, ctypes.c_int)
    _signature(gdi32, "SelectObject", HANDLE, HANDLE, HANDLE)
    _signature(gdi32, "BitBlt", BOOL, HANDLE, ctypes.c_int, ctypes.c_int,
               ctypes.c_int, ctypes.c_int, HANDLE, ctypes.c_int, ctypes.c_int, DWORD)
    _signature(gdi32, "GetDIBits", ctypes.c_int, HANDLE, HANDLE, ctypes.c_uint,
               ctypes.c_uint, ctypes.c_void_p, ctypes.POINTER(BITMAPINFOHEADER), ctypes.c_uint)
    _signature(gdi32, "DeleteObject", BOOL, HANDLE)
    _signature(gdi32, "DeleteDC", BOOL, HANDLE)


def _require_windows():
    if user32 is None:
        raise RuntimeError("Controle do computador disponível apenas no Windows.")


def _win32_error(operation):
    code = ctypes.get_last_error() if hasattr(ctypes, "get_last_error") else 0
    return OSError(code, f"{operation} falhou (Win32 {code}).")


def _integer(value, name, minimum=None, maximum=None):
    if not isinstance(value, int) or isinstance(value, bool):
        raise TypeError(f"{name} deve ser inteiro.")
    if (minimum is not None and value < minimum) or (maximum is not None and value > maximum):
        raise ValueError(f"{name} fora do intervalo permitido.")
    return value


@contextmanager
def attach_to_desktop():
    """Anexa temporariamente à área interativa e fecha o handle ao terminar."""
    _require_windows()
    previous = user32.GetThreadDesktop(kernel32.GetCurrentThreadId())
    if not previous:
        raise _win32_error("GetThreadDesktop")
    desktop = user32.OpenInputDesktop(0, False, 0x01FF)
    if not desktop:
        raise _win32_error("OpenInputDesktop")
    attached = False
    try:
        if not user32.SetThreadDesktop(desktop):
            raise _win32_error("SetThreadDesktop")
        attached = True
        yield desktop
    finally:
        try:
            if attached and not user32.SetThreadDesktop(previous):
                raise _win32_error("SetThreadDesktop (restaurar)")
        finally:
            user32.CloseDesktop(desktop)


def get_screen_size():
    _require_windows()
    width, height = user32.GetSystemMetrics(0), user32.GetSystemMetrics(1)
    if width <= 0 or height <= 0:
        raise OSError("Resolução da tela indisponível.")
    return width, height


def _mouse_pos():
    point = POINT()
    if not user32.GetCursorPos(ctypes.byref(point)):
        raise _win32_error("GetCursorPos")
    return point.x, point.y


def get_mouse_pos():
    with attach_to_desktop():
        return _mouse_pos()


def _move_to(x, y, smooth, steps):
    width, height = get_screen_size()
    x, y = max(0, min(x, width - 1)), max(0, min(y, height - 1))
    if smooth and steps > 1:
        current_x, current_y = _mouse_pos()
        for i in range(1, steps + 1):
            fraction = 0.5 * (1.0 - math.cos(i / steps * math.pi))
            if not user32.SetCursorPos(int(current_x + (x - current_x) * fraction),
                                       int(current_y + (y - current_y) * fraction)):
                raise _win32_error("SetCursorPos")
            time.sleep(0.008)
    elif not user32.SetCursorPos(x, y):
        raise _win32_error("SetCursorPos")


def move_to(x, y, smooth=True, steps=15):
    _integer(x, "x")
    _integer(y, "y")
    _integer(steps, "steps", 1, 1000)
    if not isinstance(smooth, bool):
        raise TypeError("smooth deve ser booleano.")
    with attach_to_desktop():
        _move_to(x, y, smooth, steps)


def mouse_click(x=None, y=None, button='left', clicks=1):
    buttons = {'left': (MOUSEEVENTF_LEFTDOWN, MOUSEEVENTF_LEFTUP),
               'right': (MOUSEEVENTF_RIGHTDOWN, MOUSEEVENTF_RIGHTUP),
               'middle': (MOUSEEVENTF_MIDDLEDOWN, MOUSEEVENTF_MIDDLEUP)}
    if not isinstance(button, str) or button not in buttons:
        raise ValueError("button deve ser left, right ou middle.")
    _integer(clicks, "clicks", 1, 100)
    if (x is None) != (y is None):
        raise ValueError("Informe x e y juntos.")
    if x is not None:
        _integer(x, "x")
        _integer(y, "y")
    down, up = buttons[button]
    with attach_to_desktop():
        if x is not None:
            _move_to(x, y, True, 12)
        for index in range(clicks):
            user32.mouse_event(down, 0, 0, 0, 0)
            try:
                time.sleep(0.03)
            finally:
                user32.mouse_event(up, 0, 0, 0, 0)
            if index < clicks - 1:
                time.sleep(0.08)


def mouse_scroll(amount=3):
    if isinstance(amount, bool) or not isinstance(amount, (int, float)):
        raise TypeError("amount deve ser numérico.")
    if not math.isfinite(amount) or abs(amount) > 1000:
        raise ValueError("amount fora do intervalo permitido.")
    with attach_to_desktop():
        user32.mouse_event(MOUSEEVENTF_WHEEL, 0, 0, int(amount * 120) & 0xFFFFFFFF, 0)


def _resolve_key(key_name):
    if not isinstance(key_name, str) or not key_name:
        raise ValueError("Nome da tecla inválido.")
    mapped = VK_MAPPING.get(key_name.lower())
    if mapped is not None:
        return [mapped]
    if len(key_name) != 1:
        raise ValueError(f"Tecla desconhecida: {key_name}.")
    _require_windows()
    value = user32.VkKeyScanW(key_name)
    if value == -1:
        raise ValueError(f"Tecla indisponível no layout atual: {key_name}.")
    modifiers = value >> 8
    keys = [vk for bit, vk in ((1, 0x10), (2, 0x11), (4, 0x12)) if modifiers & bit]
    return keys + [value & 0xFF]


def key_press(key_name):
    hotkey(key_name)


def hotkey(*keys):
    if not keys:
        raise ValueError("Informe ao menos uma tecla.")
    virtual_keys = []
    for key in keys:
        for vk in _resolve_key(key):
            if vk not in virtual_keys:
                virtual_keys.append(vk)
    with attach_to_desktop():
        pressed = []
        try:
            for vk in virtual_keys:
                flags = KEYEVENTF_EXTENDEDKEY if vk in _EXTENDED_KEYS else 0
                user32.keybd_event(vk, 0, flags, 0)
                pressed.append((vk, flags))
                time.sleep(0.02)
            time.sleep(0.04)
        finally:
            for vk, flags in reversed(pressed):
                user32.keybd_event(vk, 0, flags | KEYEVENTF_KEYUP, 0)


def type_text(text):
    """SendInput aceita UTF-16; caracteres fora do BMP usam dois code units."""
    if not isinstance(text, str):
        raise TypeError("text deve ser uma string.")
    encoded = text.encode("utf-16-le")
    with attach_to_desktop():
        for offset in range(0, len(encoded), 2):
            code_unit = int.from_bytes(encoded[offset:offset + 2], "little")
            events = (INPUT * 2)()
            for index, flags in enumerate((KEYEVENTF_UNICODE, KEYEVENTF_UNICODE | KEYEVENTF_KEYUP)):
                events[index].type = 1  # INPUT_KEYBOARD
                events[index].ki = KEYBDINPUT(0, code_unit, flags, 0, 0)
            if user32.SendInput(2, events, ctypes.sizeof(INPUT)) != 2:
                raise _win32_error("SendInput")
            time.sleep(0.015)


def volume_up(steps=3):
    _integer(steps, "steps", 0, 100)
    for _ in range(steps):
        key_press("vol_up")


def volume_down(steps=3):
    _integer(steps, "steps", 0, 100)
    for _ in range(steps):
        key_press("vol_down")


def volume_mute():
    key_press("vol_mute")


def media_play_pause():
    key_press("media_play_pause")


def get_active_window_title():
    with attach_to_desktop():
        hwnd = user32.GetForegroundWindow()
        if not hwnd:
            return ""
        length = user32.GetWindowTextLengthW(hwnd)
        if length <= 0:
            return ""
        buffer = ctypes.create_unicode_buffer(length + 1)
        user32.GetWindowTextW(hwnd, buffer, length + 1)
        return buffer.value


def capture_screen():
    """Captura o monitor principal em RGB e libera GDI mesmo quando há falha."""
    with attach_to_desktop():
        width, height = get_screen_size()
        screen_dc = memory_dc = bitmap = previous_bitmap = None
        try:
            screen_dc = user32.GetDC(0)
            if not screen_dc:
                raise _win32_error("GetDC")
            memory_dc = gdi32.CreateCompatibleDC(screen_dc)
            if not memory_dc:
                raise _win32_error("CreateCompatibleDC")
            bitmap = gdi32.CreateCompatibleBitmap(screen_dc, width, height)
            if not bitmap:
                raise _win32_error("CreateCompatibleBitmap")
            previous_bitmap = gdi32.SelectObject(memory_dc, bitmap)
            if not previous_bitmap or previous_bitmap == ctypes.c_void_p(-1).value:
                previous_bitmap = None
                raise _win32_error("SelectObject")
            if not gdi32.BitBlt(memory_dc, 0, 0, width, height, screen_dc, 0, 0, 0x00CC0020):
                raise _win32_error("BitBlt")
            # GetDIBits exige que o bitmap esteja fora do DC.
            if not gdi32.SelectObject(memory_dc, previous_bitmap):
                raise _win32_error("SelectObject (restaurar)")
            previous_bitmap = None
            info = BITMAPINFOHEADER()
            info.biSize = ctypes.sizeof(BITMAPINFOHEADER)
            info.biWidth, info.biHeight = width, -height
            info.biPlanes, info.biBitCount = 1, 32
            buffer = (ctypes.c_ubyte * (width * height * 4))()
            lines = gdi32.GetDIBits(screen_dc, bitmap, 0, height,
                                   ctypes.byref(buffer), ctypes.byref(info), 0)
            if lines != height:
                raise _win32_error("GetDIBits")
            return Image.frombytes("RGB", (width, height), bytes(buffer), "raw", "BGRX")
        finally:
            if previous_bitmap:
                gdi32.SelectObject(memory_dc, previous_bitmap)
            if bitmap:
                gdi32.DeleteObject(bitmap)
            if memory_dc:
                gdi32.DeleteDC(memory_dc)
            if screen_dc:
                user32.ReleaseDC(0, screen_dc)
