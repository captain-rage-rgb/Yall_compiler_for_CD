#ifndef YALL_BYTECODE_H
#define YALL_BYTECODE_H

#include <string>
#include <vector>
#include <cstdint>

/**
 * @file bytecode.h
 * @brief Bytecode instructions and program representation for the Yall Stack Machine.
 */

enum class Op {
    PUSH_CONST,
    LOAD,
    STORE,
    POP,

    // Integer operations (num, letter, flag)
    ADD_I, SUB_I, MUL_I, DIV_I, MOD_I, NEG_I,

    // Decimal operations (double)
    ADD_D, SUB_D, MUL_D, DIV_D, NEG_D,

    // Widening conversion
    I2D,

    // Comparisons
    LT_I, LE_I, GT_I, GE_I, EQ_I, NE_I,
    LT_D, LE_D, GT_D, GE_D, EQ_D, NE_D,
    EQ_S, NE_S,

    // Logical
    NOT,

    // Jumps
    JMP,
    JMP_FALSE,
    JMP_TRUE,

    // Functions
    CALL,
    RET,
    RET_VAL,

    // Output & Input
    PRINT_I,
    PRINT_D,
    PRINT_S,
    PRINT_L,
    PRINT_F,
    PRINT_SPACE,
    PRINT_NL,
    INPUT,
    INPUT_PROMPT,

    HALT
};

inline std::string opToString(Op op) {
    switch (op) {
        case Op::PUSH_CONST:   return "PUSH_CONST";
        case Op::LOAD:         return "LOAD";
        case Op::STORE:        return "STORE";
        case Op::POP:          return "POP";
        case Op::ADD_I:        return "ADD_I";
        case Op::SUB_I:        return "SUB_I";
        case Op::MUL_I:        return "MUL_I";
        case Op::DIV_I:        return "DIV_I";
        case Op::MOD_I:        return "MOD_I";
        case Op::NEG_I:        return "NEG_I";
        case Op::ADD_D:        return "ADD_D";
        case Op::SUB_D:        return "SUB_D";
        case Op::MUL_D:        return "MUL_D";
        case Op::DIV_D:        return "DIV_D";
        case Op::NEG_D:        return "NEG_D";
        case Op::I2D:          return "I2D";
        case Op::LT_I:         return "LT_I";
        case Op::LE_I:         return "LE_I";
        case Op::GT_I:         return "GT_I";
        case Op::GE_I:         return "GE_I";
        case Op::EQ_I:         return "EQ_I";
        case Op::NE_I:         return "NE_I";
        case Op::LT_D:         return "LT_D";
        case Op::LE_D:         return "LE_D";
        case Op::GT_D:         return "GT_D";
        case Op::GE_D:         return "GE_D";
        case Op::EQ_D:         return "EQ_D";
        case Op::NE_D:         return "NE_D";
        case Op::EQ_S:         return "EQ_S";
        case Op::NE_S:         return "NE_S";
        case Op::NOT:          return "NOT";
        case Op::JMP:          return "JMP";
        case Op::JMP_FALSE:    return "JMP_FALSE";
        case Op::JMP_TRUE:     return "JMP_TRUE";
        case Op::CALL:         return "CALL";
        case Op::RET:          return "RET";
        case Op::RET_VAL:      return "RET_VAL";
        case Op::PRINT_I:      return "PRINT_I";
        case Op::PRINT_D:      return "PRINT_D";
        case Op::PRINT_S:      return "PRINT_S";
        case Op::PRINT_L:      return "PRINT_L";
        case Op::PRINT_F:      return "PRINT_F";
        case Op::PRINT_SPACE:  return "PRINT_SPACE";
        case Op::PRINT_NL:     return "PRINT_NL";
        case Op::INPUT:        return "INPUT";
        case Op::INPUT_PROMPT: return "INPUT_PROMPT";
        case Op::HALT:         return "HALT";
    }
    return "UNKNOWN";
}

struct Instr {
    Op op;
    int64_t operand = 0;
    int line = 0;

    Instr(Op o, int64_t opnd, int l) : op(o), operand(opnd), line(l) {}
    Instr(Op o, int l) : op(o), operand(0), line(l) {}
};

struct ConstEntry {
    enum Kind { INT, DOUBLE, STRING } kind;
    int64_t i = 0;
    double d = 0.0;
    std::string s;
};

struct BCFunction {
    std::string name;
    int numParams = 0;
    int numSlots = 0;
    bool returnsValue = false;
    std::vector<Instr> code;
    std::vector<std::string> slotNames; // Maps slot index to variable/temp name
};

struct BCProgram {
    std::vector<BCFunction> functions;
    std::vector<ConstEntry> constants;
    int entryIndex = -1; // Index of 'start' function
};

#endif // YALL_BYTECODE_H
