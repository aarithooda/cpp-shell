#include "history.h"

class History {
private:
    std::vector<std::string> history;
    std::size_t current = 0;
    std::string current_draft;

public:
    void add(const std::string& command) {
        if (command.empty()) return;
        history.push_back(command);
    }

    std::string previous() {
        if (current == 0) return current_draft;
        current--;
        return history[current];
    }

    std::string next() {
        if (current == history.size()) return current_draft;
        if (current == history.size() - 1) {
            current++;
            return current_draft;
        }
        current++;
        return history[current];
    }

    void currentEntry(const std::string& input) {
        current_draft = input;
    }
};