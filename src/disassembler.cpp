#include "disassembler.h"
#include <iomanip>

void Disassembler::disassembleFunction(const BCProgram& program, const BCFunction& fn, std::ostream& out) {
    out << "  FUNCTION " << fn.name
        << "  (params " << fn.numParams
        << ", slots " << fn.numSlots << ")\n";

    for (size_t i = 0; i < fn.code.size(); ++i) {
        const auto& inst = fn.code[i];

        out << "    " << std::right << std::setfill('0') << std::setw(4) << i << "  "
            << std::left << std::setfill(' ') << std::setw(12) << opToString(inst.op);

        bool takesOperand = false;
        std::string comment = "";

        switch (inst.op) {
            case Op::PUSH_CONST: {
                takesOperand = true;
                if (inst.operand >= 0 && inst.operand < static_cast<int64_t>(program.constants.size())) {
                    const auto& c = program.constants[inst.operand];
                    if (c.kind == ConstEntry::INT) comment = std::to_string(c.i);
                    else if (c.kind == ConstEntry::DOUBLE) {
                        comment = std::to_string(c.d);
                        // trim trailing zeroes for clean comment if desirable
                    } else if (c.kind == ConstEntry::STRING) comment = "\"" + c.s + "\"";
                }
                break;
            }
            case Op::LOAD:
            case Op::STORE: {
                takesOperand = true;
                if (inst.operand >= 0 && inst.operand < static_cast<int64_t>(fn.slotNames.size())) {
                    comment = fn.slotNames[inst.operand];
                }
                break;
            }
            case Op::JMP:
            case Op::JMP_FALSE:
            case Op::JMP_TRUE: {
                takesOperand = true;
                comment = "-> " + std::to_string(inst.operand);
                break;
            }
            case Op::CALL: {
                takesOperand = true;
                if (inst.operand >= 0 && inst.operand < static_cast<int64_t>(program.functions.size())) {
                    comment = program.functions[inst.operand].name;
                }
                break;
            }
            default:
                break;
        }

        if (takesOperand) {
            out << std::setw(6) << inst.operand;
        } else {
            out << std::setw(6) << " ";
        }

        if (!comment.empty()) {
            out << "; " << comment;
        }
        out << "\n";
    }
}

void Disassembler::disassemble(const BCProgram& program, std::ostream& out) {
    out << "  CONSTANTS\n";
    for (size_t i = 0; i < program.constants.size(); ++i) {
        const auto& c = program.constants[i];
        out << "    " << i << ": ";
        if (c.kind == ConstEntry::INT) {
            out << "num " << c.i << "\n";
        } else if (c.kind == ConstEntry::DOUBLE) {
            out << "decimal " << c.d << "\n";
        } else if (c.kind == ConstEntry::STRING) {
            out << "text \"" << c.s << "\"\n";
        }
    }

    for (const auto& fn : program.functions) {
        disassembleFunction(program, fn, out);
    }
}
