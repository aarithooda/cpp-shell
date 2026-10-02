#include <iostream>
#include <string>

void takeInput(std::string& input) {
    std::cout << "cpp-shell> ";
    std::getline(std::cin, input);
}

void output(const std::string& input) {
    if (input == "exit") return;
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