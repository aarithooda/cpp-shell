#include <iostream>
#include <windows.h>
#include <string>
#include "process.h"
#include <filesystem>
#include <vector>
#include <sstream>

namespace fs = std::filesystem;

std::vector<std::wstring> GetPathExtension() {
    std::vector<std::wstring> extensions;
    wchar_t buffer[1024];

    DWORD len = GetEnvironmentVariableW(L"PATHEXT", buffer, 1024);
    // return len how many it put there, or return 0 if found nothing
    if (len > 0 && len < 1024) {
        std::wstringstream ss(buffer);
        std::wstring item;

        while (std::getline(ss, item, L';')) {
            if (!item.empty()) extensions.push_back(item);
        }
    }
    else {
        extensions = { L".COM", L".EXE", L".BAT", L".CMD" };
    }

    return extensions;    
}

std::wstring GetPath(const std::wstring& name) {
    wchar_t path[MAX_PATH];
    std::vector<std::wstring> extensions = GetPathExtension();

    for (const std::wstring& ext : extensions) {
        DWORD result = SearchPathW(
            NULL,
            name.c_str(),
            ext.c_str(),
            MAX_PATH,
            path,
            NULL
        );

        if (result > 0 && result <= MAX_PATH) return std::wstring(path);
    }

    WORD result = SearchPathW(
        NULL,
        name.c_str(),
        NULL,
        MAX_PATH,
        path,
        NULL
    );

    if (result > 0 && result <= MAX_PATH) return std::wstring(path);

    return L"";
}

void create_process(const std::string& name) {
    std::wstring wname(name.begin(), name.end());
    std::wstring path = GetPath(wname);

    if (path.empty()) {
        std::cout << "We could not find this file." << std::endl;
        return;
    }

    std::wstring cmdline = L"\"" + path + L"\"";
    std::vector<wchar_t> cmdBuffer(cmdline.begin(), cmdline.end());
    cmdBuffer.push_back(L'\0'); 

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    BOOL bCreateProcess = CreateProcessW(
        NULL,
        cmdBuffer.data(),
        NULL,
        NULL,
        FALSE,
        0,
        NULL,
        NULL,
        &si,
        &pi
    );

    if (!bCreateProcess) {
        std::cout << "The process could not run. Error: " << GetLastError() << std::endl;
        return;
    }

    else {
        WaitForSingleObject(pi.hProcess, INFINITE);

        DWORD exitcode = 0;

        // the process exited with a certain exit code
        if (GetExitCodeProcess(pi.hProcess, &exitcode)) {
            std::cout << "process exited with code - " << exitcode << std::endl;
        }
        //there is some error
        else {
            std::cout << "failed to get exit code. error - " << GetLastError() << std::endl;
        }

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return;
    }  
}