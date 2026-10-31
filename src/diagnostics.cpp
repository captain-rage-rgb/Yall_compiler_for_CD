#include "diagnostics.h"
#include <sstream>
#include <iomanip>

std::string Diagnostics::getSourceLine(const std::string& source, int targetLine) {
    if (targetLine <= 0) return "";
    std::istringstream stream(source);
    std::string line;
    int current = 1;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (current == targetLine) {
            return line;
        }
        current++;
    }
    return "";
}

void Diagnostics::report(std::ostream& out,
                         const std::string& source,
                         int line,
                         int col,
                         const std::string& message,
                         const std::string& phaseTag,
                         bool isWarning) {
    // 1. Message header
    if (isWarning) {
        out << "Warning: Line " << line << ", Col " << col << ": " << message;
        if (!phaseTag.empty()) {
            out << " " << phaseTag;
        }
        out << "\n";
    } else {
        out << "Line " << line << ", Col " << col << ": " << message;
        if (!phaseTag.empty()) {
            out << " " << phaseTag;
        }
        out << "\n";
    }

    // 2. Source line snippet and caret
    std::string srcLine = getSourceLine(source, line);
    if (!srcLine.empty()) {
        // Expand tabs to spaces for proper caret alignment
        std::string expanded = "";
        int caretPos = 0;
        int currentCol = 1;

        for (char c : srcLine) {
            if (currentCol == col) {
                caretPos = static_cast<int>(expanded.length());
            }
            if (c == '\t') {
                expanded += "    "; // 4 spaces per tab
                currentCol++;
            } else {
                expanded += c;
                currentCol++;
            }
        }
        if (col >= currentCol) {
            caretPos = static_cast<int>(expanded.length());
        }

        out << "  " << std::setw(5) << line << " | " << expanded << "\n";
        out << "        | ";
        if (caretPos > 0) {
            out << std::string(caretPos, ' ');
        }
        out << "^\n";
    }
}
