#include <iostream>
#include "process.h"
#include <filesystem>
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

std::wstring escapeWindowsArgument(const std::string& argument) {
    if (argument.empty()) return L"\"\"";

    bool need_quoting = false;
    for (char c : argument) {
        if (c == ' ' || c == '\t' || c == '"') {
            need_quoting = true;
            break;
        }
    }

    if (!need_quoting) return std::wstring(argument.begin(), argument.end());
    
    std::wstring escaped = L"\"";
    int backlashes = 0;

    for (char c : argument) {
        if (c == '\\') backlashes++;
        else if (c == '"') {
            //escape all backlashes and put one in the end
            escaped.append(backlashes*2 + 1, L'\\');
            escaped.push_back(L'"');
            backlashes = 0;
        }
        else {
            backlashes = 0;
            escaped.push_back(c);
        }
    }

    if (backlashes > 0) escaped.append(backlashes*2, '\\');
    
    escaped.push_back(L'"');
    return escaped;
}

ProcessResult create_process(const std::string& program, 
                            const std::vector<std::string>& arguments, 
                            std::optional<std::string> output_file = std::nullopt, 
                            bool append_output = false) {
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
        cmdline += L" " + escapeWindowsArgument(argument);
    }

    std::vector<wchar_t> cmdBuffer(cmdline.begin(), cmdline.end());
    cmdBuffer.push_back(L'\0'); 

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    HANDLE hOutputFile = INVALID_HANDLE_VALUE;

    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

    if (output_file.has_value()) {
        std::wstring woutfile(output_file->begin(), output_file->end());

        SECURITY_ATTRIBUTES sa;
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.lpSecurityDescriptor = NULL;
        sa.bInheritHandle = TRUE;

        DWORD creationDisposition = append_output ? OPEN_ALWAYS : CREATE_ALWAYS;
        DWORD desiredAccess = FILE_WRITE_DATA;
        if (append_output) {
            desiredAccess |= FILE_APPEND_DATA;
        }

        hOutputFile = CreateFileW(
            woutfile.c_str(),
            desiredAccess,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            &sa,
            creationDisposition,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );

        if (hOutputFile == INVALID_HANDLE_VALUE) {
            result.error = ProcessError::FileOpenFailed;
            return result;
        }

        if (append_output) SetFilePointer(hOutputFile, 0, NULL, FILE_END);

        si.dwFlags |= STARTF_USESTDHANDLES;
        si.hStdOutput = hOutputFile;
    }

    BOOL bCreateProcess = CreateProcessW(
        NULL,
        cmdBuffer.data(),
        NULL,
        NULL,
        output_file.has_value() ? TRUE : FALSE,
        0,
        NULL,
        NULL,
        &si,
        &pi
    );

    if (hOutputFile != INVALID_HANDLE_VALUE) CloseHandle(hOutputFile);

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