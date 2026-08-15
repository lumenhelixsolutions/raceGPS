"""Focus the UnrealEditor game window, capture its region, report luminance stats."""
import ctypes, sys, time
from ctypes import wintypes
from PIL import ImageGrab
import numpy as np

user32 = ctypes.windll.user32
out_path = sys.argv[1] if len(sys.argv) > 1 else r"C:\projects\racegps\generated\diag_game_window.png"

EnumWindowsProc = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
target = {"hwnd": None}

def cb(hwnd, lparam):
    if not user32.IsWindowVisible(hwnd):
        return True
    pid = wintypes.DWORD()
    user32.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
    try:
        name = ctypes.create_string_buffer(512)
        h = ctypes.windll.kernel32.OpenProcess(0x1000, False, pid.value)
        ctypes.windll.kernel32.QueryFullProcessImageNameA(h, 0, name, ctypes.byref(wintypes.DWORD(512)))
        ctypes.windll.kernel32.CloseHandle(h)
        exe = name.value.decode(errors="ignore").lower()
    except Exception:
        exe = ""
    if "unrealeditor" in exe:
        length = user32.GetWindowTextLengthW(hwnd)
        if length > 0:
            target["hwnd"] = hwnd
            return False
    return True

user32.EnumWindows(EnumWindowsProc(cb), 0)
if not target["hwnd"]:
    print("NO-GAME-WINDOW")
    sys.exit(2)

hwnd = target["hwnd"]
# un-minimize if needed, then force topmost + foreground
user32.ShowWindow(hwnd, 9)  # SW_RESTORE
HWND_TOPMOST = -1
SWP_NOMOVE = 0x0002
SWP_NOSIZE = 0x0001
user32.SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE)
# foreground-steal trick: attach to the foreground thread's input
fg = user32.GetForegroundWindow()
cur_tid = ctypes.windll.kernel32.GetCurrentThreadId()
fg_tid = user32.GetWindowThreadProcessId(fg, None)
user32.AttachThreadInput(cur_tid, fg_tid, True)
user32.SetForegroundWindow(hwnd)
user32.AttachThreadInput(cur_tid, fg_tid, False)
time.sleep(2.5)

rect = wintypes.RECT()
user32.GetWindowRect(hwnd, ctypes.byref(rect))
print(f"rect={rect.left},{rect.top},{rect.right},{rect.bottom}")
img = ImageGrab.grab(bbox=(rect.left, rect.top, rect.right, rect.bottom))
img.save(out_path)
a = np.asarray(img.convert("L"), dtype=float)
print(f"mean={a.mean():.2f} max={a.max():.0f} pct_gt32={(a > 32).mean() * 100:.1f} pct_gt128={(a > 128).mean() * 100:.1f}")
