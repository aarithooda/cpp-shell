#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include "process.h"
#include "tokenizer.h"
#include "parser.h"
#include <system_error>
#include <conio.h>
#include "history.h"
#include <fstream>
#include <future>
#include <algorithm>

namespace fs = std::filesystem;

std::string toUtf8(const std::wstring& value) {
    if (value.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), NULL, 0, NULL, NULL);
    if (size == 0) return "";
    std::string converted(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), converted.data(), size, NULL, NULL);
    return converted;
}

std::wstring fromUtf8(const std::string& value) {
    if (value.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), NULL, 0);
    if (size == 0) return L"";
    std::wstring converted(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), converted.data(), size);
    return converted;
}

bool takeInput(std::string& input, History& history) {
    std::string prompt = fs::current_path().u8string() + "> ";
    std::cout << prompt << std::flush;
    std::wstring edited_input;
    history.reset();
    bool in_history = false;
    size_t cursor = 0;
    size_t old_size = 0;

    auto redraw = [&]() {
        std::string text = toUtf8(edited_input);
        std::cout << '\r' << prompt << text;
        if (old_size > text.size()) std::cout << std::string(old_size-text.size(), ' ');
        std::cout << '\r' << prompt << toUtf8(edited_input.substr(0, cursor)) << std::flush;
        old_size = text.size();
    };

    while (true) {
        wint_t ch = _getwch();
        
        // for enter key
        if (ch == '\r' || ch == '\n') {
            std::cout << std::endl;
            input = toUtf8(edited_input);
            if (!input.empty()) history.add(input);
            history.reset(input);
            return true;
        }
        // EOF, ctrl+z
        else if (ch == 26) {
            std::cout << std::endl;
            return false;
        }
        // backspace
        else if (ch == '\b' || ch == 127) {
            if (cursor > 0) {
                size_t start = cursor-1;
                if (edited_input[start] >= 0xdc00 && edited_input[start] <= 0xdfff && start > 0 && edited_input[start-1] >= 0xd800 && edited_input[start-1] <= 0xdbff) start--;
                edited_input.erase(start, cursor-start);
                cursor = start;
                history.reset(toUtf8(edited_input));
                in_history = false;
                redraw();
            }
        }
        //arrow keys
        else if (ch == 0 || static_cast<unsigned char>(ch) == 224) {
            wint_t scan_code = _getwch();
            
            // up arrow
            if (scan_code == 72) {
                edited_input = fromUtf8(history.previous(toUtf8(edited_input)));
                cursor = edited_input.size();
                in_history = true;
                redraw();
            }
            //down arrow
            else if (scan_code == 80) {
                edited_input = fromUtf8(history.next());
                cursor = edited_input.size();
                in_history = false;
                redraw();
            }
            else if (scan_code == 75 && cursor > 0) {
                if (cursor > 1 && edited_input[cursor-1] >= 0xdc00 && edited_input[cursor-1] <= 0xdfff && edited_input[cursor-2] >= 0xd800 && edited_input[cursor-2] <= 0xdbff) cursor -= 2;
                else cursor--;
                redraw();
            }
            else if (scan_code == 77 && cursor < edited_input.size()) {
                if (edited_input[cursor] >= 0xd800 && edited_input[cursor] <= 0xdbff && cursor+1 < edited_input.size() && edited_input[cursor+1] >= 0xdc00 && edited_input[cursor+1] <= 0xdfff) cursor += 2;
                else cursor++;
                redraw();
            }
            else if (scan_code == 71) {
                cursor = 0;
                redraw();
            }
            else if (scan_code == 79) {
                cursor = edited_input.size();
                redraw();
            }
            else if (scan_code == 83 && cursor < edited_input.size()) {
                size_t count = 1;
                if (edited_input[cursor] >= 0xd800 && edited_input[cursor] <= 0xdbff && cursor+1 < edited_input.size() && edited_input[cursor+1] >= 0xdc00 && edited_input[cursor+1] <= 0xdfff) count = 2;
                edited_input.erase(cursor, count);
                history.reset(toUtf8(edited_input));
                in_history = false;
                redraw();
            }
        }
        // normal character
        else if (ch >= 32 && ch != 127) {
            if (in_history) {
                history.reset(toUtf8(edited_input));
                in_history = false;
            }

            edited_input.insert(edited_input.begin()+cursor, static_cast<wchar_t>(ch));
            cursor++;
            history.reset(toUtf8(edited_input));
            redraw();
        }
    }
}

bool writeCommandOutput(const Command& command, const std::string& text, int& last_status, HANDLE output_pipe = NULL) {
    if (command.output_file.has_value()) {
        auto mode = std::ios::out;
        if (command.append_output) mode |= std::ios::app;
        else mode |= std::ios::trunc;
        std::ofstream file(fs::u8path(command.output_file.value()), mode);
        if (!file.is_open()) {
            std::cerr << command.program << ": failed to open file for redirection: " << command.output_file.value() << std::endl;
            last_status = 1;
            return false;
        }
        file << text;
        file.flush();
        if (!file) {
            last_status = 1;
            return false;
        }
    }
    else if (output_pipe != NULL) {
        size_t offset = 0;
        while (offset < text.size()) {
            DWORD written = 0;
            DWORD count = static_cast<DWORD>(std::min<size_t>(text.size()-offset, MAXDWORD));
            if (!WriteFile(output_pipe, text.data()+offset, count, &written, NULL) || written == 0) {
                last_status = 1;
                return false;
            }
            offset += written;
        }
    }
    else std::cout << text;
    return true;
}

void echo(const Command& command, int& last_status, HANDLE output_pipe = NULL) {
    std::string text;
    for (size_t i = 0; i < command.arguments.size(); i++) {
        text += command.arguments[i];
        if (i+1 < command.arguments.size()) text += " ";
    }
    text += "\n";
    if (writeCommandOutput(command, text, last_status, output_pipe)) last_status = 0;
}

void changeDirectory(const std::vector<std::string>& input, int& last_status) {
    if (input.size() > 1) {
        std::cout << "cd: too many arguments" << std::endl;
        last_status = 1;
        return;
    }
    std::error_code ec;
    fs::path new_path;

    if (input.empty()) {
        const wchar_t* wide_user_profile = _wgetenv(L"USERPROFILE");
        if (wide_user_profile == nullptr) {
            std::cout << "cd: home directory is not set" << std::endl;
            last_status = 1;
            return;
        }
        new_path = wide_user_profile;
    }
    else {
        new_path = fs::u8path(input[0]);
        if (new_path == "~") {
            const wchar_t* user_profile = _wgetenv(L"USERPROFILE");
            if (user_profile == nullptr) {
                std::cout << "cd: home directory is not set" << std::endl;
                last_status = 1;
                return;
            }
            new_path = user_profile;
        }
    }

    if (!fs::is_directory(new_path, ec) || ec) {
        std::cout << "cd: directory does not exist: " << new_path.u8string() << std::endl;
        last_status = 1;
        return;
    }

    fs::current_path(new_path, ec);

    if (ec) {
        std::cout << "cd: " << ec.message() << std::endl;
        last_status = 1;
        return;
    }

    last_status = 0;
    return;
}

void printWorkingDirectory(const Command& command, int& last_status, HANDLE output_pipe = NULL) {
    const auto& input = command.arguments;
    if (!input.empty()) {
        std::cout << "pwd does not accept arguments" << std::endl;
        last_status = 1;
        return;
    }

    if (writeCommandOutput(command, fs::current_path().u8string() + "\n", last_status, output_pipe)) last_status = 0;
    return;
}

bool isBuiltin(const Command& command) {
    return command.program == "echo" || command.program == "cd" || command.program == "pwd" ||
        command.program == "set" || command.program == "help" || command.program == "exit";
}

bool runCommand(const Command& command, int& last_status, HANDLE output_pipe = NULL) {
    if (command.program.empty()) return true;
    std::ifstream input_file;
    if (isBuiltin(command) && command.input_file.has_value()) {
        input_file.open(fs::u8path(command.input_file.value()));
        if (!input_file.is_open()) {
            std::cerr << command.program << ": failed to open input file: " << command.input_file.value() << std::endl;
            last_status = 1;
            return true;
        }
    }
    std::ofstream builtin_output_file;
    if (isBuiltin(command) && command.output_file.has_value() && command.program != "echo" && command.program != "pwd" && command.program != "help") {
        auto mode = std::ios::out;
        if (command.append_output) mode |= std::ios::app;
        else mode |= std::ios::trunc;
        builtin_output_file.open(fs::u8path(command.output_file.value()), mode);
        if (!builtin_output_file.is_open()) {
            std::cerr << command.program << ": failed to open file for redirection: " << command.output_file.value() << std::endl;
            last_status = 1;
            return true;
        }
    }
    if (command.program == "exit") return false;
    if (command.program == "help") {
        std::string text = "Commands: echo, cd, pwd, set, exit, help\n";
        text += "Operators: <, >, >>, |, ;, &&, ||\n";
        text += "Variables: %NAME% or $NAME\n";
        text += "Startup settings: %USERPROFILE%\\.cpp-shellrc\n";
        if (writeCommandOutput(command, text, last_status, output_pipe)) last_status = 0;
        return true;
    }
    if (command.program == "set") {
        if (command.arguments.size() != 1) {
            std::cout << "set: expected NAME=value" << std::endl;
            last_status = 1;
            return true;
        }
        size_t equals = command.arguments[0].find('=');
        if (equals == std::string::npos || equals == 0) {
            std::cout << "set: expected NAME=value" << std::endl;
            last_status = 1;
            return true;
        }
        std::string name = command.arguments[0].substr(0, equals);
        std::string value = command.arguments[0].substr(equals+1);
        int name_size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name.data(), static_cast<int>(name.size()), NULL, 0);
        int value_size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), NULL, 0);
        std::wstring wide_name(name_size > 0 ? name_size : 0, L'\0');
        std::wstring wide_value(value_size > 0 ? value_size : 0, L'\0');
        if (name_size > 0) MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name.data(), static_cast<int>(name.size()), wide_name.data(), name_size);
        if (value_size > 0) MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), wide_value.data(), value_size);
        last_status = name_size > 0 && _wputenv_s(wide_name.c_str(), wide_value.c_str()) == 0 ? 0 : 1;
        return true;
    }
    if (command.program == "echo") {
        echo(command, last_status, output_pipe);
        return true;
    }
    else if (command.program == "cd") {
        changeDirectory(command.arguments, last_status);
        return true;
    }
    else if (command.program == "pwd") {
        printWorkingDirectory(command, last_status, output_pipe);
        return true;
    }
    else {
        ProcessResult result = create_process(command.program, command.arguments, command.input_file, command.output_file, command.append_output);

        if (result.success) {
            last_status = result.exitcode;
            return true;
        }

        switch (result.error) {
            case ProcessError::ExecutableNotFound:
                std::cerr << "Command not found: " << command.program << " (Windows error " << result.system_error << ")" << std::endl;
                last_status = 127;
                break;
            case ProcessError::LaunchFailed:
                std::cerr << "Failed to launch process: " << command.program << " (Windows error " << result.system_error << ")" << std::endl;
                last_status = 126;
                break;
            case ProcessError::WaitFailed:
                std::cerr << "Wait operation failed for: " << command.program << " (Windows error " << result.system_error << ")" << std::endl;
                last_status = 1;
                break;
            case ProcessError::ExitCodeRetrievalFailed:
                std::cerr << "Failed to retrieve exit code for: " << command.program << " (Windows error " << result.system_error << ")" << std::endl;
                last_status = 1;
                break;
            case ProcessError::FileOpenFailed:
                std::cerr << "Failed to open file for redirection (Windows error " << result.system_error << ")." << std::endl;
                last_status = 1;
                break;
            case ProcessError::None:
                last_status = 1;
                break;
        }
    }

    return true;
}

bool runPipeline(const std::vector<Token>& tokens, int& last_status) {
    std::vector<Token> expanded_tokens = tokens;
    expand_variables(expanded_tokens);
    std::vector<std::vector<Token>> command_tokens(1);
    for (const auto& token : expanded_tokens) {
        if (token.kind == TokenKind::pipe) command_tokens.emplace_back();
        else command_tokens.back().push_back(token);
    }

    for (const auto& command : command_tokens) {
        if (command.empty()) {
            std::cout << "Syntax error: expected command after pipe." << std::endl;
            last_status = 2;
            return true;
        }
    }

    std::vector<Command> commands;
    for (const auto& command : command_tokens) {
        Command parsed = parse(command, last_status);
        if (parsed.program.empty()) return true;
        commands.push_back(parsed);
    }

    if (commands.size() == 1) return runCommand(commands[0], last_status);

    struct PipeHandles {
        HANDLE input = NULL;
        HANDLE output = NULL;
    };
    std::vector<PipeHandles> pipes(commands.size()-1);
    SECURITY_ATTRIBUTES attributes{ sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    for (auto& pipe : pipes) {
        if (!CreatePipe(&pipe.input, &pipe.output, &attributes, 0)) {
            DWORD error = GetLastError();
            for (auto& open_pipe : pipes) {
                if (open_pipe.input != NULL) CloseHandle(open_pipe.input);
                if (open_pipe.output != NULL) CloseHandle(open_pipe.output);
            }
            std::cerr << "Failed to create pipe (Windows error " << error << ")." << std::endl;
            last_status = 1;
            return true;
        }
    }

    std::vector<RunningProcess> processes(commands.size());
    std::vector<std::future<std::pair<bool, int>>> builtins;
    std::vector<size_t> builtin_indexes;
    bool launch_failed = false;

    for (size_t i = 0; i < commands.size(); i++) {
        HANDLE input_pipe = i > 0 ? pipes[i-1].input : NULL;
        HANDLE output_pipe = i+1 < commands.size() ? pipes[i].output : NULL;

        if (isBuiltin(commands[i])) {
            HANDLE builtin_output = NULL;
            if (output_pipe != NULL && !commands[i].output_file.has_value()) {
                if (!DuplicateHandle(GetCurrentProcess(), output_pipe, GetCurrentProcess(), &builtin_output, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
                    launch_failed = true;
                    last_status = 1;
                    break;
                }
            }
            builtin_indexes.push_back(i);
            builtins.push_back(std::async(std::launch::async, [&, i, builtin_output]() {
                int status = 0;
                bool keep_running = true;
                if (commands[i].program == "echo" || commands[i].program == "pwd" || commands[i].program == "help") {
                    keep_running = runCommand(commands[i], status, builtin_output);
                }
                if (builtin_output != NULL) CloseHandle(builtin_output);
                return std::make_pair(keep_running, status);
            }));
        }
        else {
            processes[i] = launch_process(commands[i].program, commands[i].arguments,
                commands[i].input_file, commands[i].output_file, commands[i].append_output,
                input_pipe, output_pipe);
            if (processes[i].error != ProcessError::None) launch_failed = true;
        }
    }

    for (auto& pipe : pipes) {
        if (pipe.input != NULL) CloseHandle(pipe.input);
        if (pipe.output != NULL) CloseHandle(pipe.output);
        pipe.input = NULL;
        pipe.output = NULL;
    }

    std::vector<int> statuses(commands.size(), 0);
    bool keep_running = true;
    for (size_t i = 0; i < commands.size(); i++) {
        if (!isBuiltin(commands[i])) {
            ProcessResult result = wait_process(processes[i]);
            if (result.success) statuses[i] = static_cast<int>(result.exitcode);
            else {
                statuses[i] = result.error == ProcessError::ExecutableNotFound ? 127 : 1;
                std::cerr << "Failed to run " << commands[i].program << " (Windows error " << result.system_error << ")." << std::endl;
            }
        }
    }
    for (size_t i = 0; i < builtins.size(); i++) {
        auto result = builtins[i].get();
        statuses[builtin_indexes[i]] = result.second;
        if (!result.first) keep_running = false;
    }

    last_status = statuses.back();
    if (launch_failed && last_status == 0) last_status = 1;
    return keep_running;
}

bool output(const std::string& input, int& last_status) {
    if (input.empty() || input.find_first_not_of(" \t") == std::string::npos) return true;
    std::vector<Token> tokenised_input = tokenize(input, last_status);
    if (tokenised_input.empty()) return true;

    std::vector<Token> command_tokens;
    enum class Condition { always, success, failure };
    Condition condition = Condition::always;
    bool expect_command = false;
    for (const auto& token : tokenised_input) {
        if (token.kind == TokenKind::command_separator || token.kind == TokenKind::conditional_and || token.kind == TokenKind::conditional_or) {
            if (command_tokens.empty()) {
                std::cout << "Syntax error: expected command." << std::endl;
                last_status = 2;
                return true;
            }
            bool should_run = condition == Condition::always || (condition == Condition::success && last_status == 0) || (condition == Condition::failure && last_status != 0);
            if (should_run && !runPipeline(command_tokens, last_status)) return false;
            command_tokens.clear();
            condition = token.kind == TokenKind::conditional_and ? Condition::success : token.kind == TokenKind::conditional_or ? Condition::failure : Condition::always;
            expect_command = token.kind == TokenKind::conditional_and || token.kind == TokenKind::conditional_or;
        }
        else command_tokens.push_back(token);
    }

    if (!command_tokens.empty()) {
        bool should_run = condition == Condition::always || (condition == Condition::success && last_status == 0) || (condition == Condition::failure && last_status != 0);
        if (should_run && !runPipeline(command_tokens, last_status)) return false;
    }
    else if (expect_command) {
        std::cout << "Syntax error: expected command." << std::endl;
        last_status = 2;
    }
    return true;
}

void loadConfiguration(int& last_status) {
    const wchar_t* user_profile = _wgetenv(L"USERPROFILE");
    if (user_profile == nullptr) return;

    std::ifstream config(fs::path(user_profile) / L".cpp-shellrc");
    std::string line;
    while (std::getline(config, line)) {
        if (!line.empty()) output(line, last_status);
    }
}

int main() {
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
    History history;
    std::string input;
    int last_status = 0;
    loadConfiguration(last_status);

    while (true) {
        if (!takeInput(input, history)) {
            std::cout << "\nExiting shell..." << std::endl;
            break;
        }
    
        if (!output(input, last_status)) break;
    }

    return 0;
}
