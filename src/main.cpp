#include <iostream>
#include <string>
#include <vector>

void takeInput(std::string& input) {
    std::cout << "cpp-shell> ";
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

void output(const std::string& input) {
    if (input == "exit") return;
    if (input.empty()) return;

    std::vector<std::string> split_input = splitBySpace(input);
    if (split_input.empty()) return;
    if (split_input[0] == "echo") {
        echo(split_input);
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