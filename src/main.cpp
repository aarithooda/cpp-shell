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

namespace fs = std::filesystem;

bool takeInput(std::string& input, History& history) {
    fs::path current_dir = fs::current_path();

    std::cout << current_dir.string() << "> " << std::flush;
    
    input.clear();
    history.reset(input);
    bool in_history = false;
    char ch;

    // lambda helper to clear the input text from console
    auto clear_current_line = [&input]() {
        for (std::size_t i = 0; i < input.size(); i++) {
            std::cout << "\b \b";
        }
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
            if (!input.empty()) {
                input.pop_back();
                std::cout << "\b \b"; // moves back prints a space moves back again
                
                if (in_history) {
                    history.reset(input);
                    in_history = false;
                }
                else history.reset(input);
            }
        }
        //arrow keys
        else if (ch == 0 || static_cast<unsigned char>(ch) == 224) {
            int scan_code = _getch();
            
            // up arrow
            if (scan_code == 72) {
                clear_current_line();
                input = history.previous(input);
                std::cout << input;
                in_history = true;
            }
            //down arrow
            else if (scan_code == 80) {
                clear_current_line();
                input = history.next();
                std::cout << input;
                in_history = false;
            }
            
        }
        else if (ch == 'H' || ch == 'P') {
            std::cout << "\n[DEBUG] standalone character = " << static_cast<int>(ch) << '\n';
        }
        // normal character
        else if (ch >= 32 && ch <= 126) {
            if (in_history) {
                history.reset(input);
                in_history = false;
            }

            input.push_back(ch);
            std::cout << ch;
            history.reset(input);
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
    // expected - cd c:/users/aarit.. valid and there are no spaces.
    if (input.empty()) {
        std::cout << "Please provide path" << std::endl;
        last_status = 1;
        return;
    }
    if (input[0] == ".") {
        last_status = 0;
        return;
    }

    std::error_code ec;


    if (input[0] == "..") {
        fs::path currentPath = fs::current_path(ec);

        if (ec) {
            std::cout << "Error getting the current path: " << ec.message() << std::endl;
            last_status = 1;
            return;
        }

        fs::current_path(currentPath.parent_path(), ec);

        if (ec) {
            std::cout << "cd: " << ec.message() << std::endl;
            last_status = 1;
            return;
        }

        last_status = 0;
        return;
    }

    fs::path new_path = input[0];
    fs::path combined_path = fs::current_path(ec) / new_path;

    if (ec) {
        std::cout << "cd: " << ec.message() << std::endl;
        last_status = 1;
        return;
    }

    if (!fs::is_directory(combined_path, ec) || ec) {
        std::cout << "This directory does not exist. Please provide a valid directory." << std::endl;
        last_status = 1;
        return;
    }

    fs::current_path(combined_path, ec);

    if (ec) {
        std::cout << "cd: " << ec.message() << std::endl;
        last_status = 1;
        return;
    }

    last_status = 0;
    return;
}

void printWorkingDirectory(const std::vector<std::string>& input, int& last_status) {
    if (!input.empty()) {
        std::cout << "pwd does not accept arguments" << std::endl;
        last_status = 1;
        return;
    }

    std::cout << fs::current_path().string() << std::endl;
    last_status = 0;
    return;
}

void output(const std::string& input, int& last_status) {
    if (input.empty() || input.find_first_not_of(" \t") == std::string::npos) return;
    std::vector<Token> tokenised_input = tokenize(input, last_status);

    Command command = parse(tokenised_input, last_status);

    if (command.program == "echo") {
        echo(command, last_status);
        return;
    }
    else if (command.program == "cd") {
        changeDirectory(command.arguments, last_status);
        return;
    }
    else if (command.program == "pwd") {
        printWorkingDirectory(command.arguments, last_status);
        return;
    }
    else {
        ProcessResult result = create_process(command.program, command.arguments);

        if (result.success) {
            last_status = result.exitcode;
            return;
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
            case ProcessError::None:
                last_status = 1;
                break;
        }
    }

}

int main() {
    History history;
    std::string input;
    int last_status = 0;

    while (true) {
        if (!takeInput(input, history)) {
            std::cout << "\nExiting shell..." << std::endl;
            break;
        }
    
        if (input == "exit") {
            break;
        }
    
        output(input, last_status);
    }

    return 0;
}