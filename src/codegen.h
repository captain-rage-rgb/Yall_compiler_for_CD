#ifndef YALL_CODEGEN_H
#define YALL_CODEGEN_H

#include <string>
#include <vector>
#include <unordered_map>
#include "tac.h"
#include "bytecode.h"

/**
 * @file codegen.h
 * @brief Translates Three-Address Code (TAC) into Bytecode for the Yall Stack Machine.
 */

class BytecodeGen {
public:
    BytecodeGen() = default;

    /**
     * @brief Translates an entire TACProgram into a BCProgram.
     */
    BCProgram generate(const TACProgram& tacProg);

private:
    struct LabelPatch {
        size_t instructionIndex;
        std::string labelName;
    };

    BCProgram bcProg_;
    std::unordered_map<std::string, int> funcIndexMap_;

    // Constant pool deduplication
    int addIntConstant(int64_t val);
    int addDoubleConstant(double val);
    int addStringConstant(const std::string& val);
    int addConstantOperand(const Operand& op);

    BCFunction generateFunction(const TACFunction& tacFn);
};

#endif // YALL_CODEGEN_H
