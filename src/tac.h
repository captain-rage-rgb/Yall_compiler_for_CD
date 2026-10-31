#ifndef YALL_TAC_H
#define YALL_TAC_H

#include <string>
#include <vector>
#include "types.h"

/**
 * @file tac.h
 * @brief Three-Address Code (TAC) intermediate representation for the Yall compiler.
 */

enum class OperandKind {
    NONE,
    TEMP,
    VAR,
    CONST,
    LABEL,
    FUNC
};

struct Operand {
    OperandKind kind = OperandKind::NONE;
    Type type = Type::NOTHING;
    std::string value; // Name of temp/var/label/func, or literal value for CONST

    Operand() = default;
    Operand(OperandKind k, Type t, std::string val)
        : kind(k), type(t), value(std::move(val)) {}

    static Operand none() {
        return Operand(OperandKind::NONE, Type::NOTHING, "");
    }

    static Operand temp(const std::string& name, Type t) {
        return Operand(OperandKind::TEMP, t, name);
    }

    static Operand var(const std::string& uniqueName, Type t) {
        return Operand(OperandKind::VAR, t, uniqueName);
    }

    static Operand constant(const std::string& val, Type t) {
        return Operand(OperandKind::CONST, t, val);
    }

    static Operand label(const std::string& name) {
        return Operand(OperandKind::LABEL, Type::NOTHING, name);
    }

    static Operand func(const std::string& name) {
        return Operand(OperandKind::FUNC, Type::NOTHING, name);
    }

    bool isNone() const { return kind == OperandKind::NONE; }
    bool isTemp() const { return kind == OperandKind::TEMP; }
    bool isVar() const { return kind == OperandKind::VAR; }
    bool isConst() const { return kind == OperandKind::CONST; }
    bool isLabel() const { return kind == OperandKind::LABEL; }
    bool isFunc() const { return kind == OperandKind::FUNC; }

    bool operator==(const Operand& other) const {
        return kind == other.kind && type == other.type && value == other.value;
    }
    bool operator!=(const Operand& other) const {
        return !(*this == other);
    }
};

enum class Opcode {
    ADD, SUB, MUL, DIV, MOD,
    NEG,
    LT, LE, GT, GE, EQ, NE,
    NOT,
    COPY,
    I2D,
    LABEL,
    GOTO,
    IF_FALSE,
    IF_TRUE,
    PARAM,
    CALL,
    RETURN,
    PRINT,
    PRINT_SPACE,
    PRINT_NEWLINE,
    INPUT
};

inline std::string opcodeToString(Opcode op) {
    switch (op) {
        case Opcode::ADD: return "ADD";
        case Opcode::SUB: return "SUB";
        case Opcode::MUL: return "MUL";
        case Opcode::DIV: return "DIV";
        case Opcode::MOD: return "MOD";
        case Opcode::NEG: return "NEG";
        case Opcode::LT: return "<";
        case Opcode::LE: return "<=";
        case Opcode::GT: return ">";
        case Opcode::GE: return ">=";
        case Opcode::EQ: return "==";
        case Opcode::NE: return "!=";
        case Opcode::NOT: return "NOT";
        case Opcode::COPY: return "COPY";
        case Opcode::I2D: return "I2D";
        case Opcode::LABEL: return "LABEL";
        case Opcode::GOTO: return "GOTO";
        case Opcode::IF_FALSE: return "IF_FALSE";
        case Opcode::IF_TRUE: return "IF_TRUE";
        case Opcode::PARAM: return "PARAM";
        case Opcode::CALL: return "CALL";
        case Opcode::RETURN: return "RETURN";
        case Opcode::PRINT: return "PRINT";
        case Opcode::PRINT_SPACE: return "PRINT_SPACE";
        case Opcode::PRINT_NEWLINE: return "PRINT_NEWLINE";
        case Opcode::INPUT: return "INPUT";
    }
    return "UNKNOWN";
}

struct Instruction {
    Opcode opcode;
    Operand dest;
    Operand arg1;
    Operand arg2;
    Type type = Type::NOTHING; // Type of operation / operand
    int line = 0;

    Instruction(Opcode op, Operand d, Operand a1, Operand a2, Type t, int l)
        : opcode(op), dest(std::move(d)), arg1(std::move(a1)), arg2(std::move(a2)), type(t), line(l) {}
};

struct TACParam {
    std::string uniqueName;
    Type type;
};

struct TACFunction {
    std::string name;
    Type returnType = Type::NOTHING;
    std::vector<TACParam> params;
    std::vector<Instruction> instructions;
};

struct TACProgram {
    std::vector<TACFunction> functions;
};

#endif // YALL_TAC_H
