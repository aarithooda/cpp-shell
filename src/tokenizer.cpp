#include "tokenizer.h"
#include <iostream>
#include <cstdlib>
#include <cctype>

void addEnvironmentValue(std::string& temp, const std::string& name) {
    const char* value = std::getenv(name.c_str());
    if (value != nullptr) temp += value;
}

std::vector<Token> tokenize(const std::string& input, int& last_status) {
    if (input.empty()) return {};

    std::vector<Token> tokenisedInput;
    std::string temp;
    char quote = '\0';
    bool token_started = false;

    for (size_t i = 0; i < input.length(); i++) {
        char c = input[i];

        if ((c == '"' || c == '\'') && quote == '\0') {
            quote = c;
            token_started = true;
            continue;
        }
        if (c == quote && quote != '\0') {
            quote = '\0';
            continue;
        }

        if (c == '\\' && quote != '\'') {
            if (i+1 < input.size()) {
                temp.push_back(input[++i]);
                token_started = true;
                continue;
            }
        }

        if (quote != '\'') {
            if (c == '%' && i+1 < input.size()) {
                size_t end = input.find('%', i+1);
                if (end != std::string::npos && end > i+1) {
                    addEnvironmentValue(temp, input.substr(i+1, end-i-1));
                    token_started = true;
                    i = end;
                    continue;
                }
            }
            if (c == '$' && i+1 < input.size() && (std::isalpha(static_cast<unsigned char>(input[i+1])) || input[i+1] == '_')) {
                size_t end = i+1;
                while (end < input.size() && (std::isalnum(static_cast<unsigned char>(input[end])) || input[end] == '_')) end++;
                addEnvironmentValue(temp, input.substr(i+1, end-i-1));
                token_started = true;
                i = end-1;
                continue;
            }
        }

        if (quote == '\0') {
            if (c == ' ' || c == '\t') {
                if (token_started) {
                    tokenisedInput.push_back({TokenKind::word, temp});
                    temp.clear();
                    token_started = false;
                }
                continue;
            }

            if (c == '&' && i+1 < input.size() && input[i+1] == '&') {
                if (token_started) {
                    tokenisedInput.push_back({TokenKind::word, temp});
                    temp.clear();
                    token_started = false;
                }
                tokenisedInput.push_back({TokenKind::conditional_and, "&&"});
                i++;
                continue;
            }

            if (c == '|' && i+1 < input.size() && input[i+1] == '|') {
                if (token_started) {
                    tokenisedInput.push_back({TokenKind::word, temp});
                    temp.clear();
                    token_started = false;
                }
                tokenisedInput.push_back({TokenKind::conditional_or, "||"});
                i++;
                continue;
            }

            if (c == '|' || c == '<' || c == '>') {
                if (token_started) {
                    tokenisedInput.push_back({TokenKind::word, temp});
                    temp.clear();
                    token_started = false;
                }

                if (c == '|') tokenisedInput.push_back({TokenKind::pipe, "|"});
                else if (c == '>' && i+1 < input.size() && input[i+1] == '>') {
                    tokenisedInput.push_back({TokenKind::redirect_append, ">>"});
                    i++;
                }
                else if (c == '>') tokenisedInput.push_back({TokenKind::redirect_out, ">"});
                else if (c == '<') tokenisedInput.push_back({TokenKind::redirect_in, "<"});
                continue;
            }

            if (c == ';') {
                if (token_started) {
                    tokenisedInput.push_back({TokenKind::word, temp});
                    temp.clear();
                    token_started = false;
                }
                tokenisedInput.push_back({TokenKind::command_separator, ";"});
                continue;
            }
        }

        temp.push_back(c);
        token_started = true;
    }

    if (quote != '\0') {
        std::cout << "Syntax error: unmatched quotation mark." << std::endl;
        last_status = 1;
        return {};
    }

    if (token_started) tokenisedInput.push_back({TokenKind::word, temp});

    return tokenisedInput;
}
