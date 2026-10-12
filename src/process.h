#pragma once
#include <vector>
#include <string>
#include <windows.h>

enum class ProcessError {
    None,
    ExecutableNotFound,
    LaunchFailed,
    WaitFailed,
    ExitCodeRetrievalFailed
};

struct ProcessResult {
    bool success = false;
    DWORD exitcode = 0;
    ProcessError error = ProcessError::None;
};

ProcessResult create_process(const std::string& program, const std::vector<std::string>& arguments);