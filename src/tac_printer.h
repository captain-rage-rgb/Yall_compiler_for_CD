#ifndef YALL_TAC_PRINTER_H
#define YALL_TAC_PRINTER_H

#include <iostream>
#include "tac.h"

/**
 * @file tac_printer.h
 * @brief Pretty-printer for Three-Address Code (TAC).
 */

class TACPrinter {
public:
    static void print(const TACProgram& prog, std::ostream& out = std::cout);
    static void printFunction(const TACFunction& fn, std::ostream& out = std::cout);
    static std::string instructionToString(const Instruction& inst);
    static std::string operandToString(const Operand& op);
};

#endif // YALL_TAC_PRINTER_H
