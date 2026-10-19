#pragma once
#include <vector>
#include <string>
#include <windows.h>
#include <optional>

enum class ProcessError {
    None,
    ExecutableNotFound,
    LaunchFailed,
    WaitFailed,
    ExitCodeRetrievalFailed,
    FileOpenFailed
};

struct ProcessResult {
    bool success = false;
    DWORD exitcode = 0;
    ProcessError error = ProcessError::None;
};

ProcessResult create_process(const std::string& program, 
                            const std::vector<std::string>& arguments, 
                            std::optional<std::string> output_file = std::nullopt, 
                            bool append_output = false);