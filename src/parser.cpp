#include "parser.h"
#include <iostream>

Command parse(const std::vector<Token>& tokens, int& last_status) {
    if (tokens.empty()) return {};
    if (tokens[0].kind != TokenKind::word || tokens[0].text.empty()) {
        std::cout << "Syntax error: expected command." << std::endl;
        last_status = 2;
        return {};
    }

    Command command;
    command.program = tokens[0].text;

    for (size_t i = 1; i < tokens.size(); i++) {
        const auto& token = tokens[i];

        if (token.kind == TokenKind::redirect_out || token.kind == TokenKind::redirect_append) {
            if (command.output_file.has_value()) {
                std::cout << "Syntax error: duplicate output redirection." << std::endl;
                last_status = 2;
                return {};
            }

            if (i+1 < tokens.size() && tokens[i+1].kind == TokenKind::word) {
                command.output_file = tokens[i+1].text;
                command.append_output = (token.kind == TokenKind::redirect_append);
                i++;
            }
            else {
                std::cout << "Syntax error: expected filename after redirection operator." << std::endl;
                last_status = 2;
                return {};
            }
        }
        else if (token.kind == TokenKind::redirect_in) {
            if (command.input_file.has_value()) {
                std::cout << "Syntax error: duplicate input redirection." << std::endl;
                last_status = 2;
                return {};
            }

            if (i+1 < tokens.size() && tokens[i+1].kind == TokenKind::word) {
                command.input_file = tokens[i+1].text;
                i++;
            }
            else {
                std::cout << "Syntax error: expected filename after input redirection operator." << std::endl;
                last_status = 2;
                return {};
            }
        }
        else if (token.kind == TokenKind::word) {
            command.arguments.push_back(token.text);
        }
        else {
            std::cout << "Syntax error: unexpected operator." << std::endl;
            last_status = 2;
            return {};
        }
    }

    return command;
}
