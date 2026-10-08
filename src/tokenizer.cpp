#include "tokenizer.h"

std::vector<Token> tokenize(const std::string& input) {
    if (input.empty()) return {};

    std::vector<Token> tokenisedInput;
    std::string temp;

    for (char c : input) {
        if (c == ' ' && !temp.empty()) {
            tokenisedInput.push_back({TokenKind::word, temp});
            temp.clear();
            continue;
        }

        if (c != ' ') temp.push_back(c);
    }

    if (!temp.empty()) tokenisedInput.push_back({TokenKind::word, temp});

    return tokenisedInput;
}