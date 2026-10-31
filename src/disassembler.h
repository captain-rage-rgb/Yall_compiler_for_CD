#ifndef YALL_DISASSEMBLER_H
#define YALL_DISASSEMBLER_H

#include <iostream>
#include "bytecode.h"

/**
 * @file disassembler.h
 * @brief Bytecode disassembler for the Yall compiler.
 */

class Disassembler {
public:
    static void disassemble(const BCProgram& program, std::ostream& out = std::cout);
    static void disassembleFunction(const BCProgram& program, const BCFunction& fn, std::ostream& out = std::cout);
};

#endif // YALL_DISASSEMBLER_H
