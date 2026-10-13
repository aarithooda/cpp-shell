#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include "process.h"
#include "tokenizer.h"
#include "parser.h"
#include <system_error>

namespace fs = std::filesystem;

bool takeInput(std::string& input) {
    fs::path current_dir = fs::current_path();

    std::cout << current_dir.string() << "> ";
    
    if (std::getline(std::cin, input)) return true;
    return false;
}

void echo(const std::vector<std::string>& input, int& last_status) {
    for (int i = 0; i < input.size(); i++) {
        std::cout << input[i] << " ";
    }
    std::cout << std::endl;
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

    Command command = parse(tokenised_input);

    if (command.program == "echo") {
        echo(command.arguments, last_status);
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
    std::string input;
    int last_status = 0;

    while (true) {
        if (!takeInput(input)) {
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