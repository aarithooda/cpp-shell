#include "parser.h"

Command parse(const std::vector<Token>& tokens) {
    if (tokens.empty()) return {};

    Command command;
    command.program = tokens[0].text;
    for (size_t i = 1; i < tokens.size(); i++) {
        command.arguments.push_back(tokens[i].text);
    }

    return command;
}