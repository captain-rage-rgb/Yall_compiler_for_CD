#ifndef YALL_VM_H
#define YALL_VM_H

#include <vector>
#include <string>
#include <cstdint>
#include <iostream>
#include "bytecode.h"

/**
 * @file vm.h
 * @brief Stack-based Virtual Machine for executing Yall bytecode.
 *
 * Implements Phase 7:
 *  - Stack machine with Cell union (int64_t / double)
 *  - Heap-based frame stack (non-recursive in C++)
 *  - String handle table (strings never freed: accepted limitation)
 *  - Strict runtime error diagnostics (exit code 3)
 *  - Instruction tracing and step limits
 */

union Cell {
    int64_t i;
    double d;

    Cell() : i(0) {}
    Cell(int64_t val) : i(val) {}
    Cell(double val) : d(val) {}
};

struct Frame {
    int funcIndex = 0;
    size_t pc = 0;
    std::vector<Cell> locals;
    size_t stackBase = 0;

    Frame(int fIdx, int slots, size_t base)
        : funcIndex(fIdx), pc(0), locals(slots, Cell(int64_t(0))), stackBase(base) {}
};

class VM {
public:
    VM(const BCProgram& program);

    /**
     * @brief Executes the bytecode program.
     * @param trace If true, prints instruction trace to stderr.
     * @param maxSteps Maximum number of instructions allowed (0 = unlimited).
     * @return Exit code: 0 on success, 3 on runtime error.
     */
    int run(bool trace = false, int64_t maxSteps = 0);

private:
    const BCProgram& program_;
    std::vector<Cell> stack_;
    std::vector<Frame> callStack_;
    std::vector<std::string> stringTable_; // Global string storage

    static const size_t MAX_CALL_DEPTH = 100000;

    void runtimeError(int line, const std::string& message);
    std::string formatDouble(double d);
};

#endif // YALL_VM_H
