#ifndef YALL_DIAGNOSTICS_H
#define YALL_DIAGNOSTICS_H

#include <string>
#include <iostream>

/**
 * @file diagnostics.h
 * @brief Centralized diagnostics reporting with source code snippets and caret indicators.
 */

class Diagnostics {
public:
    static void report(std::ostream& out,
                       const std::string& source,
                       int line,
                       int col,
                       const std::string& message,
                       const std::string& phaseTag,
                       bool isWarning = false);

private:
    static std::string getSourceLine(const std::string& source, int targetLine);
};

#endif // YALL_DIAGNOSTICS_H
