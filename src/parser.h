#pragma once
#include "tokenizer.h"
#include <string>
#include <vector>

struct Command {
    std::string program;
    std::vector<std::string> arguments;
};

Command parse(const std::vector<Token>& tokens);