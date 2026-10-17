#pragma once
#include <vector>
#include <string>
#include <cstddef>

class History {
private:
    std::vector<std::string> entries;
    std::size_t index = 0;
    std::string draft;

public:
    History();

    void add(const std::string& command);

    std::string previous(const std::string& current_input);

    std::string next();

    void reset(const std::string& current_input = "");
};