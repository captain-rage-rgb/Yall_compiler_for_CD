#include "tac_printer.h"
#include <iomanip>

std::string TACPrinter::operandToString(const Operand& op) {
    if (op.isNone()) return "";
    if (op.isTemp() || op.isVar() || op.isLabel() || op.isFunc()) {
        return op.value;
    }
    if (op.isConst()) {
        if (op.type == Type::TEXT) {
            std::string esc = "\"";
            for (char c : op.value) {
                if (c == '\n') esc += "\\n";
                else if (c == '\t') esc += "\\t";
                else if (c == '"') esc += "\\\"";
                else if (c == '\\') esc += "\\\\";
                else esc += c;
            }
            esc += "\"";
            return esc;
        }
        if (op.type == Type::LETTER) {
            std::string esc = "'";
            for (char c : op.value) {
                if (c == '\n') esc += "\\n";
                else if (c == '\t') esc += "\\t";
                else if (c == '\'') esc += "\\'";
                else if (c == '\\') esc += "\\\\";
                else esc += c;
            }
            esc += "'";
            return esc;
        }
        return op.value;
    }
    return op.value;
}

std::string TACPrinter::instructionToString(const Instruction& inst) {
    if (inst.opcode == Opcode::LABEL) {
        return (inst.dest.isNone() ? inst.arg1.value : inst.dest.value) + ":";
    }

    std::string text = "    ";
    std::string d = operandToString(inst.dest);
    std::string a1 = operandToString(inst.arg1);
    std::string a2 = operandToString(inst.arg2);

    bool hasTypeTag = false;

    switch (inst.opcode) {
        case Opcode::ADD:
            text += d + " = " + a1 + " + " + a2;
            hasTypeTag = true;
            break;
        case Opcode::SUB:
            text += d + " = " + a1 + " - " + a2;
            hasTypeTag = true;
            break;
        case Opcode::MUL:
            text += d + " = " + a1 + " * " + a2;
            hasTypeTag = true;
            break;
        case Opcode::DIV:
            text += d + " = " + a1 + " / " + a2;
            hasTypeTag = true;
            break;
        case Opcode::MOD:
            text += d + " = " + a1 + " % " + a2;
            hasTypeTag = true;
            break;
        case Opcode::NEG:
            text += d + " = - " + a1;
            hasTypeTag = true;
            break;
        case Opcode::LT:
            text += d + " = " + a1 + " < " + a2;
            hasTypeTag = true;
            break;
        case Opcode::LE:
            text += d + " = " + a1 + " <= " + a2;
            hasTypeTag = true;
            break;
        case Opcode::GT:
            text += d + " = " + a1 + " > " + a2;
            hasTypeTag = true;
            break;
        case Opcode::GE:
            text += d + " = " + a1 + " >= " + a2;
            hasTypeTag = true;
            break;
        case Opcode::EQ:
            text += d + " = " + a1 + " == " + a2;
            hasTypeTag = true;
            break;
        case Opcode::NE:
            text += d + " = " + a1 + " != " + a2;
            hasTypeTag = true;
            break;
        case Opcode::NOT:
            text += d + " = NOT " + a1;
            break;
        case Opcode::COPY:
            text += d + " = " + a1;
            break;
        case Opcode::I2D:
            text += d + " = I2D " + a1;
            break;
        case Opcode::GOTO:
            text += "GOTO " + a1;
            break;
        case Opcode::IF_FALSE:
            text += "IF_FALSE " + a1 + " GOTO " + a2;
            break;
        case Opcode::IF_TRUE:
            text += "IF_TRUE " + a1 + " GOTO " + a2;
            break;
        case Opcode::PARAM:
            text += "PARAM " + a1;
            break;
        case Opcode::CALL:
            if (!inst.dest.isNone()) {
                text += d + " = CALL " + a1 + ", " + a2;
            } else {
                text += "CALL " + a1 + ", " + a2;
            }
            break;
        case Opcode::RETURN:
            if (!inst.arg1.isNone()) {
                text += "RETURN " + a1;
            } else {
                text += "RETURN";
            }
            break;
        case Opcode::PRINT:
            text += "PRINT " + a1;
            hasTypeTag = true;
            break;
        case Opcode::PRINT_SPACE:
            text += "PRINT_SPACE";
            break;
        case Opcode::PRINT_NEWLINE:
            text += "PRINT_NEWLINE";
            break;
        case Opcode::INPUT:
            if (!inst.arg1.isNone()) {
                text += d + " = INPUT " + a1;
            } else {
                text += d + " = INPUT";
            }
            break;
        case Opcode::LABEL:
            break;
    }

    if (hasTypeTag && inst.type != Type::NOTHING && inst.type != Type::ERROR) {
        int pad = 22 - static_cast<int>(text.length());
        if (pad < 2) pad = 2;
        text.append(pad, ' ');
        text += "[" + typeToString(inst.type) + "]";
    }

    return text;
}

void TACPrinter::printFunction(const TACFunction& fn, std::ostream& out) {
    out << "FUNCTION " << fn.name << "(";
    for (size_t i = 0; i < fn.params.size(); ++i) {
        if (i > 0) out << ", ";
        out << fn.params[i].uniqueName << " : " << typeToString(fn.params[i].type);
    }
    out << ") -> " << typeToString(fn.returnType) << "\n";

    for (const auto& inst : fn.instructions) {
        out << instructionToString(inst) << "\n";
    }
}

void TACPrinter::print(const TACProgram& prog, std::ostream& out) {
    for (size_t i = 0; i < prog.functions.size(); ++i) {
        printFunction(prog.functions[i], out);
        if (i + 1 < prog.functions.size()) {
            out << "\n";
        }
    }
}
