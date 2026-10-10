#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include "process.h"
#include "tokenizer.h"
#include "parser.h"

namespace fs = std::filesystem;

void takeInput(std::string& input) {
    fs::path current_dir = fs::current_path();

    std::cout << current_dir.string() << "> ";
    std::getline(std::cin, input);
}

void echo(const std::vector<std::string>& input) {
    for (int i = 0; i < input.size(); i++) {
        std::cout << input[i] << " ";
    }
    std::cout << std::endl;
}

void changeDirectory(const std::vector<std::string>& input) {
    // expected - cd c:/users/aarit.. valid and there are no spaces.
    if (input.empty()) {
        std::cout << "Please provide path" << std::endl;
        return;
    }
    if (input[0] == ".") return;
    if (input[0] == "..") {
        fs::path currentPath = fs::current_path();
        fs::current_path(currentPath.parent_path());
        return;
    }

    fs::path new_path = input[0];
    fs::path combined_path = fs::current_path() / new_path;
    if (!fs::is_directory(combined_path)) {
        std::cout << "This directory does not exist. Please provide a valid directory." << std::endl;
        return;
    }

    fs::current_path(combined_path);
}

void printWorkingDirectory(const std::vector<std::string>& input) {
    if (!input.empty()) {
        std::cout << "pwd does not accept arguments" << std::endl;
        return;
    }

    std::cout << fs::current_path().string() << std::endl;
    return;
}

void output(const std::string& input) {
    if (input.empty()) return;
    std::vector<Token> tokenised_input = tokenize(input);

    Command command = parse(tokenised_input);

    if (command.program == "echo") {
        echo(command.arguments);
        return;
    }
    else if (command.program == "cd") {
        changeDirectory(command.arguments);
        return;
    }
    else if (command.program == "pwd") {
        printWorkingDirectory(command.arguments);
        return;
    }
    else {
        // currently only name of program is useful
        create_process(command.program);
        return;
    }

}

int main() {
    std::string input;
    while (input != "exit") {
        takeInput(input);
        output(input);
    }

    return 0;
}