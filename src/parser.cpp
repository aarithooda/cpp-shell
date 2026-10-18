#include "parser.h"

Command parse(const std::vector<Token>& tokens) {
    if (tokens.empty()) return {};

    Command command;
    command.program = tokens[0].text;

    for (size_t i = 1; i < tokens.size(); i++) {
        const auto& token = tokens[i];

        if (token.kind == TokenKind::redirect_out || token.kind == TokenKind::redirect_append) {
            if (i+1 < tokens.size() && tokens[i+1].kind == TokenKind::word) {
                command.output_file = tokens[i+1].text;
                command.append_output = (token.kind == TokenKind::redirect_append);
                i++;
            }
        }
        else if (token.kind == TokenKind::redirect_in) {
            if (i+1 < tokens.size() && tokens[i+1].kind == TokenKind::word) {
                command.input_file = tokens[i+1].text;
                i++;
            }
        }
        else if (token.kind == TokenKind::word) {
            command.arguments.push_back(token.text);
        }
    }

    return command;
}