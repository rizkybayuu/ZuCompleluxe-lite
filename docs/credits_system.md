# Credits System Architecture

This document describes the developer credits system introduced by `zumacplxcredit.dll`, the sidecar DLL that extends Zuma Deluxe's credits screen with custom entries, interactive elements, and fullscreen compatibility fixes.

---

## 1. Overview

The credits system is implemented across three components:

| Component | Description |
|---|---|
| **Cave A** — Credits Table | A static table of credit entries embedded in the `.zcc` PE section, read by the credits renderer at startup |
| **Cave B** — Per-Line Draw Hook | An assembly cave that intercepts each credit line draw call to apply custom formatting |
| **Cave C** — Mouse Click Hook | An assembly cave that intercepts `Widget::MouseDown` to detect GitHub button clicks |

The `.zcc` PE section is injected into `Zuma.exe` by `builder/patch_zccredit.py`. This section contains the trampolines for Cave B and Cave C, plus the `ZTable` descriptor pointing to `zumacplxcredit.dll`.

---

## 2. Credits Table (Cave A)

Cave A is a data table of credit entries, each with:

```c
struct CreditEntry {
    int   type;     // 0=text, 1=button
    int   spacing;  // vertical spacing in pixels (vanilla resolution units)
    char *text;     // display string (or URL for type=1)
};
```

### Extended Entries (39–41)

The following entries are appended after the last vanilla credit:

| Index | Type | Text | Spacing |
|---|---|---|---|
| 39 | text | `ZuCompleluxe Project` | 50 (default) |
| 40 | text | `Rizky Bayuu "Lamp?x"` | 35 |
| 41 | button | GitHub URL | 60 |

Entry 41 is the interactive GitHub button rendered with a custom highlight sprite. Its `spacing=60` ensures sufficient visual separation from the credit text above it.

---

## 3. Per-Line Draw Hook (Cave B)

Cave B patches the vanilla credits draw loop to call `ZCC_DrawLine` for each entry before rendering. This allows the DLL to:

- Apply custom foreground/background colors per-entry type.
- Inject the GitHub button sprite at entry 41's Y position.
- Skip vanilla rendering for button-type entries (replaced entirely by the custom sprite).

**Export**: `ZCC_DrawLine(int index, int x, int y, const char *text)`

---

## 4. Mouse Click Hook (Cave C) and the `__thiscall` Crash

Cave C intercepts `Widget::MouseDown`, which uses the `__thiscall` calling convention — the `this` pointer is passed in `ecx` and `edx` may contain a second implicit argument.

### The Problem

`ZCC_OnMouseDown` is a C `__cdecl` function. Calling it without saving `ecx`/`edx` first causes both registers to be clobbered. The vanilla fallback instruction immediately after the call is:

```asm
mov byte ptr [ecx+0x1f], 1   ; uses ecx as Widget* this
```

If `ecx` is corrupted by the `__cdecl` call, this write targets an invalid address and causes:

```
Exception: Access Violation (code 0xc0000005)
```

This was the crash observed at address `0x00592082` in `Zuma.exe`.

### The Fix

`patch_zccredit.py` generates Cave C with explicit register preservation:

```asm
; Cave C — patched version
push edx                   ; save edx (may hold implicit arg or scratch)
push ecx                   ; save ecx (__thiscall 'this' pointer)

; push args for ZCC_OnMouseDown (x, y from stack; adjust offsets for two saved regs)
push [esp+0x10]            ; arg: y  (offset +8 for ecx+edx on stack)
push [esp+0x10]            ; arg: x
call ZCC_OnMouseDown       ; __cdecl — clobbers ecx, edx freely

pop ecx                    ; restore 'this'
pop edx                    ; restore edx

; vanilla fallback
mov byte ptr [ecx+0x1f], 1 ; now ecx is valid again
```

**Export**: `ZCC_OnMouseDown(int x, int y)` — returns non-zero if the click hit the GitHub button.

---

## 5. GitHub Button in Fullscreen

In the original implementation, `ShellExecuteA` was called without a parent HWND and without minimizing the game window. In fullscreen mode, the DirectDraw exclusive surface remained on top, making the launched browser invisible behind it.

### Fix in `zumacplxcredit.dll.c`

`open_github_url_()` now:

1. Retrieves the game's HWND via `FindWindowA("MainWindow", NULL)`.
2. Calls `ShellExecuteA(hwnd, "open", url, NULL, NULL, SW_SHOWNORMAL)`.
3. Immediately calls `ShowWindow(hwnd, SW_MINIMIZE)` to iconify the fullscreen window, bringing the browser to the foreground.

This works in both windowed and fullscreen mode.

---

## 6. Credits Scrolling Loop Fix

### The Problem

The vanilla credits loop uses a hardcoded reset Y value of `520` (the approximate height of the 640×480 screen plus a small margin). At non-480p heights, the credits text would "pop in" at the wrong Y coordinate instead of smoothly scrolling up from below the viewport.

### The Fix

`zumareshandler.dll.c` applies two AOB patches at runtime:

| Patch Name | Vanilla Value | Patched Value | Effect |
|---|---|---|---|
| `CreditsScrollStartY` | `520` | `TARGET_H + 20` | Credits start scrolling from just below the actual screen bottom |
| `CreditsScrollResetY` | `520` | `TARGET_H + 20` | Credits reset to just below the screen bottom when looping |

The AOB signature for these patches is located in `Zuma.exe` at the file offset containing `83 FF 9C 7D 0A 8B 4D ?? C7 41 5C 08 02 00 00`. The DWORD at offset `+11` in this pattern holds the hardcoded `520` value and is overwritten at load time.

With this fix, the credits loop is fully seamless: the last entry scrolls off the top, and the first entry re-enters from below the viewport — no pop-in, no gap.

---

## 7. Coordinate Patches for Credits Screen

The following UI elements in the credits screen are also repositioned by `zumareshandler.dll` at the target resolution:

| Patch Name | Description |
|---|---|
| `CreditsWhateverBtnH` | Height of the "whatever" (dismiss) button, scaled to `SDIM_VAL` |
| `CreditsWhateverBtnX` | X position of the dismiss button, scaled to `SX_VAL` |
| `CreditsTextCenterX` | Center X for credit text rendering, set to `content_w / 2 + X_offset` |
