#include "tokenizer.h"
#include <iostream>

std::vector<Token> tokenize(const std::string& input, int& last_status) {
    if (input.empty()) return {};

    std::vector<Token> tokenisedInput;
    std::string temp;
    bool in_quotes = false;
    bool token_started = false;

    for (size_t i = 0; i < input.length(); i++) {
        char c = input[i];

        if (c == '"' && !in_quotes) {
            in_quotes = true;
            token_started = true;
            continue;
        }
        if (c == '"' && in_quotes) {
            in_quotes = false;
            continue;
        }

        if (!in_quotes) {
            if (c == ' ' || c == '\t') {
                if (token_started) {
                    tokenisedInput.push_back({TokenKind::word, temp});
                    temp.clear();
                    token_started = false;
                }
                continue;
            }

            if (c == '<' || c == '>') {
                if (token_started) {
                    tokenisedInput.push_back({TokenKind::word, temp});
                    temp.clear();
                    token_started = false;
                }

                if (c == '>' && i+1 < input.size() && input[i+1] == '>') {
                    tokenisedInput.push_back({TokenKind::redirect_append, ">>"});
                    i++;
                }
                else if (c == '>') tokenisedInput.push_back({TokenKind::redirect_out, ">"});
                else if (c == '<') tokenisedInput.push_back({TokenKind::redirect_in, "<"});
                continue;
            }
        }

        temp.push_back(c);
        token_started = true;
    }

    if (in_quotes) {
        std::cout << "Syntax error: unmatched quotation mark." << std::endl;
        last_status = 1;
        return {};
    }

    if (token_started) tokenisedInput.push_back({TokenKind::word, temp});

    return tokenisedInput;
}