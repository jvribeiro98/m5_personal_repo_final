# -*- coding: utf-8 -*-
"""
Controlador de Sistema Operacional Windows (Computer Use)
- Movimento suave e cliques de mouse
- Digitação e atalhos de teclado
- Controle de volume e multimídia do PC
- Gerenciamento de janelas e resolução
- Captura de tela em alta velocidade (GDI Win32 ~25ms)
"""

import ctypes
from ctypes import wintypes
import time
import math
from PIL import Image

user32 = ctypes.windll.user32
gdi32 = ctypes.windll.gdi32

# Constantes de Mouse
MOUSEEVENTF_MOVE = 0x0001
MOUSEEVENTF_LEFTDOWN = 0x0002
MOUSEEVENTF_LEFTUP = 0x0004
MOUSEEVENTF_RIGHTDOWN = 0x0008
MOUSEEVENTF_RIGHTUP = 0x0010
MOUSEEVENTF_MIDDLEDOWN = 0x0020
MOUSEEVENTF_MIDDLEUP = 0x0040
MOUSEEVENTF_WHEEL = 0x0800
MOUSEEVENTF_ABSOLUTE = 0x8000

# Constantes de Teclado
KEYEVENTF_EXTENDEDKEY = 0x0001
KEYEVENTF_KEYUP = 0x0002
KEYEVENTF_UNICODE = 0x0004

VK_MAPPING = {
    'enter': 0x0D,
    'esc': 0x1B,
    'escape': 0x1B,
    'tab': 0x09,
    'space': 0x20,
    'espaço': 0x20,
    'backspace': 0x08,
    'delete': 0x2E,
    'del': 0x2E,
    'up': 0x26,
    'down': 0x28,
    'left': 0x25,
    'right': 0x27,
    'home': 0x24,
    'end': 0x23,
    'pageup': 0x21,
    'pagedown': 0x22,
    'ctrl': 0x11,
    'control': 0x11,
    'alt': 0x12,
    'shift': 0x10,
    'win': 0x5B,
    'windows': 0x5B,
    'vol_up': 0xAF,
    'vol_down': 0xAE,
    'vol_mute': 0xAD,
    'media_play_pause': 0xB3,
    'f': 0x46,
    'k': 0x4B,
    'm': 0x4D,
    't': 0x54,
    'w': 0x57,
    'f1': 0x70,
    'f2': 0x71,
    'f3': 0x72,
    'f4': 0x73,
    'f5': 0x74,
    'f6': 0x75,
    'f11': 0x7A,
    'f12': 0x7B,
    'browser_back': 0xA6,
    'browser_forward': 0xA7,
    'browser_refresh': 0xA8,
}

class POINT(ctypes.Structure):
    _fields_ = [('x', ctypes.c_long), ('y', ctypes.c_long)]

def attach_to_desktop():
    """Garante que a thread atual tem acesso ao desktop interativo do usuário."""
    try:
        hDesk = user32.OpenInputDesktop(0, False, 0x01FF)
        if hDesk:
            user32.SetThreadDesktop(hDesk)
            return hDesk
    except Exception:
        pass
    return None

def get_screen_size():
    return user32.GetSystemMetrics(0), user32.GetSystemMetrics(1)

def get_mouse_pos():
    attach_to_desktop()
    pt = POINT()
    user32.GetCursorPos(ctypes.byref(pt))
    return pt.x, pt.y

def move_to(x, y, smooth=True, steps=15):
    """Move o cursor até (x, y) de forma suave ou instantânea."""
    attach_to_desktop()
    cur_x, cur_y = get_mouse_pos()
    target_x = max(0, min(x, user32.GetSystemMetrics(0) - 1))
    target_y = max(0, min(y, user32.GetSystemMetrics(1) - 1))

    if not smooth or steps <= 1:
        user32.SetCursorPos(target_x, target_y)
        return

    # Interpolação suave cúbica (Ease-in-out)
    for i in range(1, steps + 1):
        t = i / float(steps)
        ease_t = 0.5 * (1.0 - math.cos(t * math.pi))
        cx = int(cur_x + (target_x - cur_x) * ease_t)
        cy = int(cur_y + (target_y - cur_y) * ease_t)
        user32.SetCursorPos(cx, cy)
        time.sleep(0.008)

    user32.SetCursorPos(target_x, target_y)

def mouse_click(x=None, y=None, button='left', clicks=1):
    """Executa clique na posição atual ou em (x, y)."""
    attach_to_desktop()
    if x is not None and y is not None:
        move_to(x, y, smooth=True, steps=12)
        time.sleep(0.04)

    for _ in range(clicks):
        if button == 'left':
            user32.mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0)
            time.sleep(0.03)
            user32.mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0)
        elif button == 'right':
            user32.mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, 0)
            time.sleep(0.03)
            user32.mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, 0)
        elif button == 'middle':
            user32.mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, 0)
            time.sleep(0.03)
            user32.mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, 0)

        if clicks > 1:
            time.sleep(0.08)

def mouse_scroll(amount=3):
    """Rola a página (positivo = cima, negativo = baixo)."""
    attach_to_desktop()
    wheel_delta = int(amount * 120)
    user32.mouse_event(MOUSEEVENTF_WHEEL, 0, 0, wheel_delta, 0)

def key_press(key_name):
    """Pressiona e solta uma tecla."""
    attach_to_desktop()
    vk = VK_MAPPING.get(key_name.lower())
    if vk is None and len(key_name) == 1:
        vk = user32.VkKeyScanW(ord(key_name)) & 0xFF

    if vk:
        user32.keybd_event(vk, 0, 0, 0)
        time.sleep(0.03)
        user32.keybd_event(vk, 0, KEYEVENTF_KEYUP, 0)

def hotkey(*keys):
    """Dispara combinação de teclas (ex: hotkey('ctrl', 't'))."""
    attach_to_desktop()
    vks = []
    for k in keys:
        vk = VK_MAPPING.get(k.lower())
        if vk is None and len(k) == 1:
            vk = user32.VkKeyScanW(ord(k)) & 0xFF
        if vk:
            vks.append(vk)

    for vk in vks:
        user32.keybd_event(vk, 0, 0, 0)
        time.sleep(0.02)

    time.sleep(0.04)

    for vk in reversed(vks):
        user32.keybd_event(vk, 0, KEYEVENTF_KEYUP, 0)
        time.sleep(0.02)

def type_text(text):
    """Digita texto no campo focado caractere a caractere."""
    attach_to_desktop()
    for ch in text:
        user32.keybd_event(0, ord(ch), KEYEVENTF_UNICODE, 0)
        user32.keybd_event(0, ord(ch), KEYEVENTF_UNICODE | KEYEVENTF_KEYUP, 0)
        time.sleep(0.015)

def volume_up(steps=3):
    attach_to_desktop()
    for _ in range(steps):
        user32.keybd_event(0xAF, 0, 0, 0)
        user32.keybd_event(0xAF, 0, KEYEVENTF_KEYUP, 0)
        time.sleep(0.02)

def volume_down(steps=3):
    attach_to_desktop()
    for _ in range(steps):
        user32.keybd_event(0xAE, 0, 0, 0)
        user32.keybd_event(0xAE, 0, KEYEVENTF_KEYUP, 0)
        time.sleep(0.02)

def volume_mute():
    attach_to_desktop()
    user32.keybd_event(0xAD, 0, 0, 0)
    user32.keybd_event(0xAD, 0, KEYEVENTF_KEYUP, 0)

def media_play_pause():
    attach_to_desktop()
    user32.keybd_event(0xB3, 0, 0, 0)
    user32.keybd_event(0xB3, 0, KEYEVENTF_KEYUP, 0)

def get_active_window_title():
    attach_to_desktop()
    hwnd = user32.GetForegroundWindow()
    length = user32.GetWindowTextLengthW(hwnd)
    if length > 0:
        buf = ctypes.create_unicode_buffer(length + 1)
        user32.GetWindowTextW(hwnd, buf, length + 1)
        return buf.value
    return ""

def capture_screen():
    """Captura a tela inteira em RGB via GDI Win32 em ~25ms."""
    w = user32.GetSystemMetrics(0)
    h = user32.GetSystemMetrics(1)

    hdc_screen = user32.GetDC(0)
    hdc_mem = gdi32.CreateCompatibleDC(hdc_screen)
    hbm = gdi32.CreateCompatibleBitmap(hdc_screen, w, h)
    gdi32.SelectObject(hdc_mem, hbm)

    SRCCOPY = 0x00CC0020
    gdi32.BitBlt(hdc_mem, 0, 0, w, h, hdc_screen, 0, 0, SRCCOPY)

    class BITMAPINFOHEADER(ctypes.Structure):
        _fields_ = [
            ('biSize', wintypes.DWORD),
            ('biWidth', wintypes.LONG),
            ('biHeight', wintypes.LONG),
            ('biPlanes', wintypes.WORD),
            ('biBitCount', wintypes.WORD),
            ('biCompression', wintypes.DWORD),
            ('biSizeImage', wintypes.DWORD),
            ('biXPelsPerMeter', wintypes.LONG),
            ('biYPelsPerMeter', wintypes.LONG),
            ('biClrUsed', wintypes.DWORD),
            ('biClrImportant', wintypes.DWORD)
        ]

    bmi = BITMAPINFOHEADER()
    bmi.biSize = ctypes.sizeof(BITMAPINFOHEADER)
    bmi.biWidth = w
    bmi.biHeight = -h
    bmi.biPlanes = 1
    bmi.biBitCount = 32
    bmi.biCompression = 0

    buf = (ctypes.c_ubyte * (w * h * 4))()
    gdi32.GetDIBits(hdc_mem, hbm, 0, h, ctypes.byref(buf), ctypes.byref(bmi), 0)

    gdi32.DeleteObject(hbm)
    gdi32.DeleteDC(hdc_mem)
    user32.ReleaseDC(0, hdc_screen)

    return Image.frombuffer('RGBA', (w, h), bytes(buf), 'raw', 'BGRA', 0, 1).convert('RGB')
