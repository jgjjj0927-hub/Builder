// language: C++, file: src/keyboard.cpp
#include "keyboard.h"

static HHOOK g_hook = nullptr;

static LRESULT CALLBACK kb_proc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
        auto* kb = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        DWORD vk = kb->vkCode;
        bool allowed = (vk >= '0' && vk <= '9') || vk == VK_RETURN || vk == VK_BACK || vk == VK_TAB;
        if (!allowed) return 1;
    }
    return CallNextHookEx(g_hook, nCode, wParam, lParam);
}

static DWORD WINAPI lock_thread(LPVOID) {
    g_hook = SetWindowsHookExW(WH_KEYBOARD_LL, kb_proc, GetModuleHandleW(nullptr), 0);
    if (!g_hook) return 1;
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}

void start_keyboard_lock() {
    CreateThread(nullptr, 0, lock_thread, nullptr, 0, nullptr);
}
