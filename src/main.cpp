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
#include <cstdlib>
#include <map>

namespace fs = std::filesystem;

bool takeInput(std::string& input, History& history) {
    std::string prompt = fs::current_path().string() + "> ";
    std::cout << prompt << std::flush;
    input.clear();
    history.reset(input);
    bool in_history = false;
    size_t cursor = 0;
    size_t old_size = 0;
    char ch;

    auto redraw = [&]() {
        std::cout << '\r' << prompt << input;
        if (old_size > input.size()) std::cout << std::string(old_size-input.size(), ' ');
        std::cout << '\r' << prompt << input.substr(0, cursor) << std::flush;
        old_size = input.size();
    };

    while (true) {
        ch = _getch();
        
        // for enter key
        if (ch == '\r' || ch == '\n') {
            std::cout << std::endl;
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
                input.erase(cursor-1, 1);
                cursor--;
                history.reset(input);
                in_history = false;
                redraw();
            }
        }
        //arrow keys
        else if (ch == 0 || static_cast<unsigned char>(ch) == 224) {
            int scan_code = _getch();
            
            // up arrow
            if (scan_code == 72) {
                input = history.previous(input);
                cursor = input.size();
                in_history = true;
                redraw();
            }
            //down arrow
            else if (scan_code == 80) {
                input = history.next();
                cursor = input.size();
                in_history = false;
                redraw();
            }
            else if (scan_code == 75 && cursor > 0) {
                cursor--;
                redraw();
            }
            else if (scan_code == 77 && cursor < input.size()) {
                cursor++;
                redraw();
            }
            else if (scan_code == 71) {
                cursor = 0;
                redraw();
            }
            else if (scan_code == 79) {
                cursor = input.size();
                redraw();
            }
            else if (scan_code == 83 && cursor < input.size()) {
                input.erase(cursor, 1);
                history.reset(input);
                in_history = false;
                redraw();
            }
        }
        // normal character
        else if (ch >= 32 && ch <= 126) {
            if (in_history) {
                history.reset(input);
                in_history = false;
            }

            input.insert(input.begin()+cursor, ch);
            cursor++;
            history.reset(input);
            redraw();
        }
    }
}

void echo(const Command& command, int& last_status) {
    std::ostream* out = &std::cout;
    std::ofstream file;

    if (command.output_file.has_value()) {
        auto mode = std::ios::out;
        if (command.append_output) mode |= std::ios::app;
        else mode |= std::ios::trunc;

        file.open(command.output_file.value(), mode);
        if (!file.is_open()) {
            std::cerr << "echo: failed to open file for redirection: " << command.output_file.value() << std::endl;
            last_status = 1;
            return;
        }
        out = &file;
    }

    for (size_t i = 0; i < command.arguments.size(); i++) {
        *out << command.arguments[i];
        if (i+1 < command.arguments.size()) *out << " ";
    }
    *out << std::endl;
    last_status = 0;
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
        const char* user_profile = std::getenv("USERPROFILE");
        if (user_profile == nullptr) {
            std::cout << "cd: home directory is not set" << std::endl;
            last_status = 1;
            return;
        }
        new_path = user_profile;
    }
    else {
        new_path = input[0];
        if (new_path == "~") {
            const char* user_profile = std::getenv("USERPROFILE");
            if (user_profile == nullptr) {
                std::cout << "cd: home directory is not set" << std::endl;
                last_status = 1;
                return;
            }
            new_path = user_profile;
        }
    }

    if (!fs::is_directory(new_path, ec) || ec) {
        std::cout << "cd: directory does not exist: " << new_path.string() << std::endl;
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

void printWorkingDirectory(const std::vector<std::string>& input, const std::optional<std::string>& output_file, int& last_status) {
    if (!input.empty()) {
        std::cout << "pwd does not accept arguments" << std::endl;
        last_status = 1;
        return;
    }

    std::ostream* out = &std::cout;
    std::ofstream file;
    if (output_file.has_value()) {
        file.open(output_file.value());
        if (!file.is_open()) {
            std::cerr << "pwd: failed to open file for redirection: " << output_file.value() << std::endl;
            last_status = 1;
            return;
        }
        out = &file;
    }
    *out << fs::current_path().string() << std::endl;
    last_status = 0;
    return;
}

bool runCommand(const Command& command, int& last_status) {
    if (command.program.empty()) return true;
    if (command.program == "exit") return false;
    if (command.program == "help") {
        std::cout << "Commands: echo, cd, pwd, set, exit, help" << std::endl;
        std::cout << "Operators: <, >, >>, |, ;, &&, ||" << std::endl;
        std::cout << "Variables: %NAME% or $NAME" << std::endl;
        std::cout << "Startup settings: %USERPROFILE%\\.cpp-shellrc" << std::endl;
        last_status = 0;
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
        last_status = _putenv_s(name.c_str(), value.c_str()) == 0 ? 0 : 1;
        return true;
    }
    if (command.program == "echo") {
        echo(command, last_status);
        return true;
    }
    else if (command.program == "cd") {
        changeDirectory(command.arguments, last_status);
        return true;
    }
    else if (command.program == "pwd") {
        printWorkingDirectory(command.arguments, command.output_file, last_status);
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
                std::cout << "Command not found: " << command.program << std::endl;
                last_status = 127;
                break;
            case ProcessError::LaunchFailed:
                std::cout << "Failed to launch process: " << command.program << std::endl;
                last_status = 126;
                break;
            case ProcessError::WaitFailed:
                std::cout << "Wait operation failed for: " << command.program << std::endl;
                last_status = 1;
                break;
            case ProcessError::ExitCodeRetrievalFailed:
                std::cout << "Failed to retrieve exit code for: " << command.program << std::endl;
                last_status = 1;
                break;
            case ProcessError::FileOpenFailed:
                std::cout << "Failed to open file for redirection." << std::endl;
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
    std::vector<std::vector<Token>> command_tokens(1);
    for (const auto& token : tokens) {
        if (token.kind == TokenKind::pipe) command_tokens.emplace_back();
        else command_tokens.back().push_back(token);
    }

    for (const auto& command : command_tokens) {
        if (command.empty()) {
            std::cout << "Syntax error: expected command after pipe." << std::endl;
            last_status = 1;
            return true;
        }
    }

    std::vector<Command> commands;
    for (const auto& command : command_tokens) {
        Command parsed = parse(command, last_status);
        if (parsed.program.empty()) return true;
        commands.push_back(parsed);
    }

    static unsigned int pipe_number = 0;
    std::vector<fs::path> temporary_files;
    for (size_t i = 0; i < commands.size(); i++) {
        if (i > 0 && !commands[i].input_file.has_value()) commands[i].input_file = temporary_files.back().string();
        if (i+1 < commands.size()) {
            fs::path temp_file = fs::temp_directory_path() / ("cpp-shell-pipe-" + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(pipe_number++) + ".tmp");
            temporary_files.push_back(temp_file);
            std::ofstream create_file(temp_file, std::ios::trunc);
            create_file.close();
            if (!commands[i].output_file.has_value()) commands[i].output_file = temp_file.string();
        }
        if (!runCommand(commands[i], last_status)) {
            for (const auto& file : temporary_files) fs::remove(file);
            return false;
        }
    }

    for (const auto& file : temporary_files) fs::remove(file);
    return true;
}

bool output(const std::string& input, int& last_status) {
    if (input.empty() || input.find_first_not_of(" \t") == std::string::npos) return true;
    std::vector<Token> tokenised_input = tokenize(input, last_status);
    if (tokenised_input.empty()) return true;

    std::vector<Token> command_tokens;
    enum class Condition { always, success, failure };
    Condition condition = Condition::always;
    for (const auto& token : tokenised_input) {
        if (token.kind == TokenKind::command_separator || token.kind == TokenKind::conditional_and || token.kind == TokenKind::conditional_or) {
            if (!command_tokens.empty()) {
                bool should_run = condition == Condition::always || (condition == Condition::success && last_status == 0) || (condition == Condition::failure && last_status != 0);
                if (should_run && !runPipeline(command_tokens, last_status)) return false;
                command_tokens.clear();
            }
            else if (token.kind != TokenKind::command_separator) {
                std::cout << "Syntax error: expected command." << std::endl;
                last_status = 1;
            }
            condition = token.kind == TokenKind::conditional_and ? Condition::success : token.kind == TokenKind::conditional_or ? Condition::failure : Condition::always;
        }
        else command_tokens.push_back(token);
    }

    if (!command_tokens.empty()) {
        bool should_run = condition == Condition::always || (condition == Condition::success && last_status == 0) || (condition == Condition::failure && last_status != 0);
        if (should_run && !runPipeline(command_tokens, last_status)) return false;
    }
    return true;
}

void loadConfiguration(int& last_status) {
    const char* user_profile = std::getenv("USERPROFILE");
    if (user_profile == nullptr) return;

    std::ifstream config(fs::path(user_profile) / ".cpp-shellrc");
    std::string line;
    while (std::getline(config, line)) {
        if (!line.empty()) output(line, last_status);
    }
}

int main() {
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
