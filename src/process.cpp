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

ProcessResult create_process(const std::string& program, const std::vector<std::string>& arguments) {
    std::wstring wname(program.begin(), program.end());
    std::wstring path = GetPath(wname);
    ProcessResult result;

    // could not find the file
    if (path.empty()) {
        result.error = ProcessError::ExecutableNotFound;
        return result;
    }

    std::wstring cmdline = L"\"" + path + L"\"";
    for (const auto& argument : arguments) {
        std::wstring warg(argument.begin(), argument.end());
        cmdline += L" " + warg;
    }

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

    // the process could not run
    if (!bCreateProcess) {
        result.error = ProcessError::LaunchFailed;
        return result;
    }

    DWORD waitresult = WaitForSingleObject(pi.hProcess, INFINITE);

    if (waitresult != WAIT_OBJECT_0) {
        result.error = ProcessError::WaitFailed;  

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return result;
    }


    DWORD exitcode = 0;

    //the process ran but there is some error
    if (!GetExitCodeProcess(pi.hProcess, &exitcode)) {
        result.error = ProcessError::ExitCodeRetrievalFailed;

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        return result;
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    result.success = true;
    result.exitcode = exitcode;

    return result;
}