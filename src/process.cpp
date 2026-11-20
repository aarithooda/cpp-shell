#include "process.h"
#include <sstream>
#include <vector>

class ProcessHandleGuard {
public:
    HANDLE value = NULL;

    ProcessHandleGuard() = default;
    explicit ProcessHandleGuard(HANDLE handle) : value(handle) {}
    ProcessHandleGuard(const ProcessHandleGuard&) = delete;
    ProcessHandleGuard& operator=(const ProcessHandleGuard&) = delete;

    ProcessHandleGuard(ProcessHandleGuard&& other) noexcept : value(other.release()) {}
    ProcessHandleGuard& operator=(ProcessHandleGuard&& other) noexcept {
        if (this != &other) {
            if (value != NULL && value != INVALID_HANDLE_VALUE) CloseHandle(value);
            value = other.release();
        }
        return *this;
    }

    ~ProcessHandleGuard() {
        if (value != NULL && value != INVALID_HANDLE_VALUE) CloseHandle(value);
    }

    HANDLE release() {
        HANDLE handle = value;
        value = NULL;
        return handle;
    }
};

RunningProcess::~RunningProcess() {
    if (process != NULL) CloseHandle(process);
    if (thread != NULL) CloseHandle(thread);
}

RunningProcess::RunningProcess(RunningProcess&& other) noexcept
    : process(other.process), thread(other.thread), error(other.error), system_error(other.system_error) {
    other.process = NULL;
    other.thread = NULL;
}

RunningProcess& RunningProcess::operator=(RunningProcess&& other) noexcept {
    if (this != &other) {
        if (process != NULL) CloseHandle(process);
        if (thread != NULL) CloseHandle(thread);
        process = other.process;
        thread = other.thread;
        error = other.error;
        system_error = other.system_error;
        other.process = NULL;
        other.thread = NULL;
    }
    return *this;
}

std::wstring toWide(const std::string& value) {
    if (value.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), NULL, 0);
    if (size == 0) return L"";
    std::wstring converted(size, L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), converted.data(), size);
    return converted;
}

std::vector<std::wstring> getPathExtensions() {
    DWORD size = GetEnvironmentVariableW(L"PATHEXT", NULL, 0);
    if (size == 0) return { L".COM", L".EXE", L".BAT", L".CMD" };

    std::wstring value(size, L'\0');
    DWORD length = GetEnvironmentVariableW(L"PATHEXT", value.data(), size);
    if (length == 0 || length >= size) return { L".COM", L".EXE", L".BAT", L".CMD" };
    value.resize(length);

    std::vector<std::wstring> extensions;
    std::wstringstream stream(value);
    std::wstring extension;
    while (std::getline(stream, extension, L';')) {
        if (!extension.empty()) extensions.push_back(extension);
    }
    return extensions;
}

std::wstring getPath(const std::wstring& name) {
    std::vector<wchar_t> path(32768);
    for (const auto& extension : getPathExtensions()) {
        DWORD length = SearchPathW(NULL, name.c_str(), extension.c_str(), static_cast<DWORD>(path.size()), path.data(), NULL);
        if (length > 0 && length < path.size()) return path.data();
    }

    DWORD length = SearchPathW(NULL, name.c_str(), NULL, static_cast<DWORD>(path.size()), path.data(), NULL);
    if (length > 0 && length < path.size()) return path.data();
    return L"";
}

std::wstring escapeWindowsArgument(const std::wstring& argument) {
    if (argument.empty()) return L"\"\"";

    bool need_quoting = false;
    for (wchar_t c : argument) {
        if (c == L' ' || c == L'\t' || c == L'\"') {
            need_quoting = true;
            break;
        }
    }
    if (!need_quoting) return argument;

    std::wstring escaped = L"\"";
    size_t backslashes = 0;
    for (wchar_t c : argument) {
        if (c == L'\\') backslashes++;
        else if (c == L'\"') {
            escaped.append(backslashes*2+1, L'\\');
            escaped.push_back(L'\"');
            backslashes = 0;
        }
        else {
            escaped.append(backslashes, L'\\');
            backslashes = 0;
            escaped.push_back(c);
        }
    }
    escaped.append(backslashes*2, L'\\');
    escaped.push_back(L'\"');
    return escaped;
}

bool duplicateHandle(HANDLE source, ProcessHandleGuard& copy) {
    if (source == NULL || source == INVALID_HANDLE_VALUE) return false;
    HANDLE handle = NULL;
    if (!DuplicateHandle(GetCurrentProcess(), source, GetCurrentProcess(), &handle, 0, TRUE, DUPLICATE_SAME_ACCESS)) return false;
    copy.value = handle;
    return true;
}

RunningProcess launch_process(const std::string& program,
                            const std::vector<std::string>& arguments,
                            std::optional<std::string> input_file,
                            std::optional<std::string> output_file,
                            bool append_output,
                            HANDLE input_pipe,
                            HANDLE output_pipe) {
    RunningProcess result;
    std::wstring wide_name = toWide(program);
    if (!program.empty() && wide_name.empty()) {
        result.error = ProcessError::LaunchFailed;
        result.system_error = ERROR_NO_UNICODE_TRANSLATION;
        return result;
    }
    std::wstring path = getPath(wide_name);
    if (path.empty()) {
        result.error = ProcessError::ExecutableNotFound;
        result.system_error = GetLastError();
        return result;
    }

    std::wstring command_line = L"\"" + path + L"\"";
    for (const auto& argument : arguments) {
        std::wstring wide_argument = toWide(argument);
        if (!argument.empty() && wide_argument.empty()) {
            result.error = ProcessError::LaunchFailed;
            result.system_error = ERROR_NO_UNICODE_TRANSLATION;
            return result;
        }
        command_line += L" " + escapeWindowsArgument(wide_argument);
    }
    std::vector<wchar_t> command_buffer(command_line.begin(), command_line.end());
    command_buffer.push_back(L'\0');

    ProcessHandleGuard input_handle;
    ProcessHandleGuard output_handle;
    ProcessHandleGuard error_handle;
    ProcessHandleGuard input_file_handle;
    ProcessHandleGuard output_file_handle;

    if (!duplicateHandle(GetStdHandle(STD_INPUT_HANDLE), input_handle) ||
        !duplicateHandle(GetStdHandle(STD_OUTPUT_HANDLE), output_handle) ||
        !duplicateHandle(GetStdHandle(STD_ERROR_HANDLE), error_handle)) {
        result.error = ProcessError::LaunchFailed;
        result.system_error = GetLastError();
        return result;
    }

    if (input_pipe != NULL) {
        ProcessHandleGuard pipe_copy;
        if (!duplicateHandle(input_pipe, pipe_copy)) {
            result.error = ProcessError::LaunchFailed;
            result.system_error = GetLastError();
            return result;
        }
        input_handle = std::move(pipe_copy);
    }

    if (output_pipe != NULL) {
        ProcessHandleGuard pipe_copy;
        if (!duplicateHandle(output_pipe, pipe_copy)) {
            result.error = ProcessError::LaunchFailed;
            result.system_error = GetLastError();
            return result;
        }
        output_handle = std::move(pipe_copy);
    }

    if (input_file.has_value()) {
        std::wstring filename = toWide(input_file.value());
        if (filename.empty()) {
            result.error = ProcessError::LaunchFailed;
            result.system_error = ERROR_NO_UNICODE_TRANSLATION;
            return result;
        }
        SECURITY_ATTRIBUTES attributes{ sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
        input_file_handle.value = CreateFileW(filename.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
            &attributes, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (input_file_handle.value == INVALID_HANDLE_VALUE) {
            input_file_handle.value = NULL;
            result.error = ProcessError::FileOpenFailed;
            result.system_error = GetLastError();
            return result;
        }
        input_handle = std::move(input_file_handle);
    }

    if (output_file.has_value()) {
        std::wstring filename = toWide(output_file.value());
        if (filename.empty()) {
            result.error = ProcessError::LaunchFailed;
            result.system_error = ERROR_NO_UNICODE_TRANSLATION;
            return result;
        }
        SECURITY_ATTRIBUTES attributes{ sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
        DWORD access = GENERIC_WRITE;
        DWORD disposition = append_output ? OPEN_ALWAYS : CREATE_ALWAYS;
        output_file_handle.value = CreateFileW(filename.c_str(), access, FILE_SHARE_READ | FILE_SHARE_WRITE,
            &attributes, disposition, FILE_ATTRIBUTE_NORMAL, NULL);
        if (output_file_handle.value == INVALID_HANDLE_VALUE) {
            output_file_handle.value = NULL;
            result.error = ProcessError::FileOpenFailed;
            result.system_error = GetLastError();
            return result;
        }
        if (append_output) {
            LARGE_INTEGER zero{};
            if (!SetFilePointerEx(output_file_handle.value, zero, NULL, FILE_END)) {
                result.error = ProcessError::FileOpenFailed;
                result.system_error = GetLastError();
                return result;
            }
        }
        output_handle = std::move(output_file_handle);
    }

    HANDLE inherited_handles[] = { input_handle.value, output_handle.value, error_handle.value };
    SIZE_T attribute_size = 0;
    InitializeProcThreadAttributeList(NULL, 1, 0, &attribute_size);
    std::vector<unsigned char> attribute_buffer(attribute_size);
    auto* attributes = reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>(attribute_buffer.data());
    if (!InitializeProcThreadAttributeList(attributes, 1, 0, &attribute_size)) {
        result.error = ProcessError::LaunchFailed;
        result.system_error = GetLastError();
        return result;
    }
    if (!UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherited_handles,
        sizeof(inherited_handles), NULL, NULL)) {
        result.error = ProcessError::LaunchFailed;
        result.system_error = GetLastError();
        DeleteProcThreadAttributeList(attributes);
        return result;
    }

    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof(STARTUPINFOEXW);
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdInput = input_handle.value;
    startup.StartupInfo.hStdOutput = output_handle.value;
    startup.StartupInfo.hStdError = error_handle.value;
    startup.lpAttributeList = attributes;
    PROCESS_INFORMATION process{};
    BOOL created = CreateProcessW(path.c_str(), command_buffer.data(), NULL, NULL, TRUE,
        EXTENDED_STARTUPINFO_PRESENT, NULL, NULL, &startup.StartupInfo, &process);
    DWORD system_error = created ? ERROR_SUCCESS : GetLastError();
    DeleteProcThreadAttributeList(attributes);

    if (!created) {
        result.error = ProcessError::LaunchFailed;
        result.system_error = system_error;
        return result;
    }

    result.process = process.hProcess;
    result.thread = process.hThread;
    return result;
}

ProcessResult wait_process(RunningProcess& process) {
    ProcessResult result;
    if (process.error != ProcessError::None) {
        result.error = process.error;
        result.system_error = process.system_error;
        return result;
    }

    DWORD wait_result = WaitForSingleObject(process.process, INFINITE);
    if (wait_result != WAIT_OBJECT_0) {
        result.error = ProcessError::WaitFailed;
        result.system_error = GetLastError();
    }
    else if (!GetExitCodeProcess(process.process, &result.exitcode)) {
        result.error = ProcessError::ExitCodeRetrievalFailed;
        result.system_error = GetLastError();
    }
    else result.success = true;

    if (process.process != NULL) CloseHandle(process.process);
    if (process.thread != NULL) CloseHandle(process.thread);
    process.process = NULL;
    process.thread = NULL;
    return result;
}

ProcessResult create_process(const std::string& program,
                            const std::vector<std::string>& arguments,
                            std::optional<std::string> input_file,
                            std::optional<std::string> output_file,
                            bool append_output) {
    RunningProcess process = launch_process(program, arguments, input_file, output_file, append_output);
    return wait_process(process);
}
