#include "history.h"

History::History() {
    index = 0;
}

void History::add(const std::string& command) {
    if (command.empty()) return;
    entries.push_back(command);
    index = entries.size(); // Point past the end for the next new command
}

std::string History::previous(const std::string& current_input) {
    if (entries.empty()) return current_input;

    // If we are currently at the draft position, save the user's current typing
    if (index == entries.size()) {
        draft = current_input;
    }

    // Move backward if possible
    if (index > 0) {
        index--;
    }

    return entries[index];
}

std::string History::next() {
    if (entries.empty()) return "";

    // Move forward if possible
    if (index < entries.size()) {
        index++;
    }

    // If we've navigated past the end of history, return the saved draft
    if (index == entries.size()) {
        return draft;
    }

    return entries[index];
}

void History::reset(const std::string& current_input) {
    index = entries.size();
    draft = current_input;
}