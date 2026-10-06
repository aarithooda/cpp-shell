#include <iostream>
#include <windows.h>
#include <string>
#include "process.h"

void create_process(const std::string& name) {
    std::wstring wname(name.begin(), name.end());
    wchar_t path[MAX_PATH];

    DWORD result = SearchPathW(
        NULL,
        wname.c_str(),
        L".exe",
        MAX_PATH,
        path,
        NULL
    );

    // we have the path we can create the process
    if (result > 0 && result <= MAX_PATH) {
        STARTUPINFOW si;
        PROCESS_INFORMATION pi;

        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        ZeroMemory(&pi, sizeof(pi));

        BOOL bCreateProcess = CreateProcessW(
            path,
            NULL,
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
            std::cout << "The process could not run. Error: " << GetLastError() << std::endl;
            return;
        }

        // the process ran
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

    // the path is too long
    else if (result > MAX_PATH) {
        std::cout << "The path is too long. Buffer was too small." <<std::endl;
        return;
    }

    // we could not find the path
    else {
        std::cout << "could not find this file." << std::endl;
        return;
    }
    

}