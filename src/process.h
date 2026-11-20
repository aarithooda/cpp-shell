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
    DWORD system_error = ERROR_SUCCESS;
};

struct RunningProcess {
    HANDLE process = NULL;
    HANDLE thread = NULL;
    ProcessError error = ProcessError::None;
    DWORD system_error = ERROR_SUCCESS;

    RunningProcess() = default;
    ~RunningProcess();
    RunningProcess(const RunningProcess&) = delete;
    RunningProcess& operator=(const RunningProcess&) = delete;
    RunningProcess(RunningProcess&& other) noexcept;
    RunningProcess& operator=(RunningProcess&& other) noexcept;
};

RunningProcess launch_process(const std::string& program,
                            const std::vector<std::string>& arguments,
                            std::optional<std::string> input_file,
                            std::optional<std::string> output_file,
                            bool append_output,
                            HANDLE input_pipe = NULL,
                            HANDLE output_pipe = NULL);
ProcessResult wait_process(RunningProcess& process);

ProcessResult create_process(const std::string& program, 
                            const std::vector<std::string>& arguments, 
                            std::optional<std::string> input_file = std::nullopt,
                            std::optional<std::string> output_file = std::nullopt, 
                            bool append_output = false);
