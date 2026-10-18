#pragma once
#include "tokenizer.h"
#include <string>
#include <vector>
#include <optional>

struct Command {
    std::string program;
    std::vector<std::string> arguments;
    std::optional<std::string> output_file;
    std::optional<std::string> input_file;
    bool append_output = false;
};

Command parse(const std::vector<Token>& tokens);