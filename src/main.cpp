#include <iostream>
#include <string>
#include <vector>
#include <filesystem>

void takeInput(std::string& input) {
    std::filesystem::path current_dir = std::filesystem::current_path();

    std::cout << current_dir.string() << "> ";
    std::getline(std::cin, input);
}

std::vector<std::string> splitBySpace(const std::string& input) {
    if (input.empty()) return {};

    std::vector<std::string> split_input;
    std::string temp;
    for (char c : input) {
        if (c == ' ' && !temp.empty()) {
            split_input.push_back(temp);
            temp.clear();
            continue;
        } 
        
        if (c != ' ') temp.push_back(c);
    }
    if (!temp.empty()) split_input.push_back(temp);

    return split_input;
}

void echo(const std::vector<std::string>& input) {
    for (int i = 1; i < input.size(); i++) {
        std::cout << input[i] << " ";
    }
    std::cout << std::endl;
}

void changeDirectory(const std::vector<std::string>& input) {
    // expected - cd c:/users/aarit.. valid and there are no spaces.
    if (input.size() == 1) {
        std::cout << "Please provide path" << std::endl;
        return;
    }
    if (input[1] == ".") return;
    if (input[1] == "..") {
        std::filesystem::path currentPath = std::filesystem::current_path();
        std::filesystem::current_path(currentPath.parent_path());
        return;
    }

    std::filesystem::path new_path = input[1];
    std::filesystem::path combined_path = std::filesystem::current_path() / new_path;
    if (!std::filesystem::is_directory(combined_path)) {
        std::cout << "This directory does not exist. Please provide a valid directory." << std::endl;
        return;
    }

    std::filesystem::current_path(combined_path);
}

void output(const std::string& input) {
    if (input == "exit") return;
    if (input.empty()) return;

    std::vector<std::string> split_input = splitBySpace(input);
    if (split_input.empty()) return;
    if (split_input[0] == "echo") {
        echo(split_input);
        return;
    }

    if (split_input[0] == "cd") {
        changeDirectory(split_input);
        return;
    }

    std::cout << "You entered: " << input << std::endl;
}

int main() {
    std::string input;
    while (input != "exit") {
        takeInput(input);
        output(input);
    }

    return 0;
}