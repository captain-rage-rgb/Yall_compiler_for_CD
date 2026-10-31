#include "codegen.h"
#include <iostream>

int BytecodeGen::addIntConstant(int64_t val) {
    for (size_t i = 0; i < bcProg_.constants.size(); ++i) {
        if (bcProg_.constants[i].kind == ConstEntry::INT && bcProg_.constants[i].i == val) {
            return static_cast<int>(i);
        }
    }
    ConstEntry entry;
    entry.kind = ConstEntry::INT;
    entry.i = val;
    bcProg_.constants.push_back(entry);
    return static_cast<int>(bcProg_.constants.size() - 1);
}

int BytecodeGen::addDoubleConstant(double val) {
    for (size_t i = 0; i < bcProg_.constants.size(); ++i) {
        if (bcProg_.constants[i].kind == ConstEntry::DOUBLE && bcProg_.constants[i].d == val) {
            return static_cast<int>(i);
        }
    }
    ConstEntry entry;
    entry.kind = ConstEntry::DOUBLE;
    entry.d = val;
    bcProg_.constants.push_back(entry);
    return static_cast<int>(bcProg_.constants.size() - 1);
}

int BytecodeGen::addStringConstant(const std::string& val) {
    for (size_t i = 0; i < bcProg_.constants.size(); ++i) {
        if (bcProg_.constants[i].kind == ConstEntry::STRING && bcProg_.constants[i].s == val) {
            return static_cast<int>(i);
        }
    }
    ConstEntry entry;
    entry.kind = ConstEntry::STRING;
    entry.s = val;
    bcProg_.constants.push_back(entry);
    return static_cast<int>(bcProg_.constants.size() - 1);
}

int BytecodeGen::addConstantOperand(const Operand& op) {
    if (op.type == Type::NUM) {
        try {
            int64_t val = std::stoll(op.value);
            return addIntConstant(val);
        } catch (...) {
            return addIntConstant(0);
        }
    }
    if (op.type == Type::DECIMAL) {
        try {
            double val = std::stod(op.value);
            return addDoubleConstant(val);
        } catch (...) {
            return addDoubleConstant(0.0);
        }
    }
    if (op.type == Type::FLAG) {
        int64_t val = (op.value == "yes") ? 1 : 0;
        return addIntConstant(val);
    }
    if (op.type == Type::LETTER) {
        int64_t val = 0;
        if (!op.value.empty()) {
            val = static_cast<unsigned char>(op.value[0]);
        }
        return addIntConstant(val);
    }
    if (op.type == Type::TEXT) {
        return addStringConstant(op.value);
    }
    return addIntConstant(0);
}

BCProgram BytecodeGen::generate(const TACProgram& tacProg) {
    bcProg_.constants.clear();
    bcProg_.functions.clear();
    funcIndexMap_.clear();

    // Register all functions to obtain their indices
    for (size_t i = 0; i < tacProg.functions.size(); ++i) {
        funcIndexMap_[tacProg.functions[i].name] = static_cast<int>(i);
        if (tacProg.functions[i].name == "start") {
            bcProg_.entryIndex = static_cast<int>(i);
        }
    }

    // Generate bytecode for each function
    for (const auto& tacFn : tacProg.functions) {
        bcProg_.functions.push_back(generateFunction(tacFn));
    }

    return bcProg_;
}

BCFunction BytecodeGen::generateFunction(const TACFunction& tacFn) {
    BCFunction bcFn;
    bcFn.name = tacFn.name;
    bcFn.numParams = static_cast<int>(tacFn.params.size());
    bcFn.returnsValue = (tacFn.returnType != Type::NOTHING);

    std::unordered_map<std::string, int> slotMap;
    auto getSlot = [&](const std::string& name) -> int {
        auto it = slotMap.find(name);
        if (it != slotMap.end()) return it->second;
        int slot = static_cast<int>(bcFn.slotNames.size());
        slotMap[name] = slot;
        bcFn.slotNames.push_back(name);
        return slot;
    };

    // Slots 0..numParams-1 hold parameters
    for (const auto& p : tacFn.params) {
        getSlot(p.uniqueName);
    }

    auto pushOperand = [&](const Operand& op, int line) {
        if (op.isConst()) {
            int constIdx = addConstantOperand(op);
            bcFn.code.emplace_back(Op::PUSH_CONST, constIdx, line);
        } else if (op.isVar() || op.isTemp()) {
            int slot = getSlot(op.value);
            bcFn.code.emplace_back(Op::LOAD, slot, line);
        }
    };

    auto storeOperand = [&](const Operand& dest, int line) {
        int slot = getSlot(dest.value);
        bcFn.code.emplace_back(Op::STORE, slot, line);
    };

    std::unordered_map<std::string, int64_t> labelAddrs;
    std::vector<LabelPatch> patches;

    for (const auto& inst : tacFn.instructions) {
        switch (inst.opcode) {
            case Opcode::LABEL: {
                std::string labelName = inst.dest.isNone() ? inst.arg1.value : inst.dest.value;
                labelAddrs[labelName] = static_cast<int64_t>(bcFn.code.size());
                break;
            }
            case Opcode::COPY: {
                pushOperand(inst.arg1, inst.line);
                storeOperand(inst.dest, inst.line);
                break;
            }
            case Opcode::I2D: {
                pushOperand(inst.arg1, inst.line);
                bcFn.code.emplace_back(Op::I2D, inst.line);
                storeOperand(inst.dest, inst.line);
                break;
            }
            case Opcode::NEG: {
                pushOperand(inst.arg1, inst.line);
                Op op = (inst.type == Type::DECIMAL) ? Op::NEG_D : Op::NEG_I;
                bcFn.code.emplace_back(op, inst.line);
                storeOperand(inst.dest, inst.line);
                break;
            }
            case Opcode::NOT: {
                pushOperand(inst.arg1, inst.line);
                bcFn.code.emplace_back(Op::NOT, inst.line);
                storeOperand(inst.dest, inst.line);
                break;
            }
            case Opcode::ADD:
            case Opcode::SUB:
            case Opcode::MUL:
            case Opcode::DIV:
            case Opcode::MOD: {
                pushOperand(inst.arg1, inst.line);
                pushOperand(inst.arg2, inst.line);
                Op op = Op::ADD_I;
                if (inst.type == Type::DECIMAL) {
                    if (inst.opcode == Opcode::ADD) op = Op::ADD_D;
                    else if (inst.opcode == Opcode::SUB) op = Op::SUB_D;
                    else if (inst.opcode == Opcode::MUL) op = Op::MUL_D;
                    else if (inst.opcode == Opcode::DIV) op = Op::DIV_D;
                } else {
                    if (inst.opcode == Opcode::ADD) op = Op::ADD_I;
                    else if (inst.opcode == Opcode::SUB) op = Op::SUB_I;
                    else if (inst.opcode == Opcode::MUL) op = Op::MUL_I;
                    else if (inst.opcode == Opcode::DIV) op = Op::DIV_I;
                    else if (inst.opcode == Opcode::MOD) op = Op::MOD_I;
                }
                bcFn.code.emplace_back(op, inst.line);
                storeOperand(inst.dest, inst.line);
                break;
            }
            case Opcode::LT:
            case Opcode::LE:
            case Opcode::GT:
            case Opcode::GE:
            case Opcode::EQ:
            case Opcode::NE: {
                pushOperand(inst.arg1, inst.line);
                pushOperand(inst.arg2, inst.line);
                Op op = Op::EQ_I;
                if (inst.type == Type::DECIMAL) {
                    if (inst.opcode == Opcode::LT) op = Op::LT_D;
                    else if (inst.opcode == Opcode::LE) op = Op::LE_D;
                    else if (inst.opcode == Opcode::GT) op = Op::GT_D;
                    else if (inst.opcode == Opcode::GE) op = Op::GE_D;
                    else if (inst.opcode == Opcode::EQ) op = Op::EQ_D;
                    else if (inst.opcode == Opcode::NE) op = Op::NE_D;
                } else if (inst.type == Type::TEXT) {
                    if (inst.opcode == Opcode::EQ) op = Op::EQ_S;
                    else if (inst.opcode == Opcode::NE) op = Op::NE_S;
                } else {
                    if (inst.opcode == Opcode::LT) op = Op::LT_I;
                    else if (inst.opcode == Opcode::LE) op = Op::LE_I;
                    else if (inst.opcode == Opcode::GT) op = Op::GT_I;
                    else if (inst.opcode == Opcode::GE) op = Op::GE_I;
                    else if (inst.opcode == Opcode::EQ) op = Op::EQ_I;
                    else if (inst.opcode == Opcode::NE) op = Op::NE_I;
                }
                bcFn.code.emplace_back(op, inst.line);
                storeOperand(inst.dest, inst.line);
                break;
            }
            case Opcode::GOTO: {
                patches.push_back({bcFn.code.size(), inst.arg1.value});
                bcFn.code.emplace_back(Op::JMP, 0, inst.line);
                break;
            }
            case Opcode::IF_FALSE: {
                pushOperand(inst.arg1, inst.line);
                patches.push_back({bcFn.code.size(), inst.arg2.value});
                bcFn.code.emplace_back(Op::JMP_FALSE, 0, inst.line);
                break;
            }
            case Opcode::IF_TRUE: {
                pushOperand(inst.arg1, inst.line);
                patches.push_back({bcFn.code.size(), inst.arg2.value});
                bcFn.code.emplace_back(Op::JMP_TRUE, 0, inst.line);
                break;
            }
            case Opcode::PARAM: {
                pushOperand(inst.arg1, inst.line);
                break;
            }
            case Opcode::CALL: {
                int fIdx = funcIndexMap_[inst.arg1.value];
                bcFn.code.emplace_back(Op::CALL, fIdx, inst.line);
                if (!inst.dest.isNone()) {
                    storeOperand(inst.dest, inst.line);
                } else {
                    // Callee returns value but statement discards it
                    if (inst.type != Type::NOTHING && inst.type != Type::ERROR) {
                        bcFn.code.emplace_back(Op::POP, inst.line);
                    }
                }
                break;
            }
            case Opcode::RETURN: {
                if (!inst.arg1.isNone()) {
                    pushOperand(inst.arg1, inst.line);
                    bcFn.code.emplace_back(Op::RET_VAL, inst.line);
                } else {
                    bcFn.code.emplace_back(Op::RET, inst.line);
                }
                break;
            }
            case Opcode::PRINT: {
                pushOperand(inst.arg1, inst.line);
                Op printOp = Op::PRINT_I;
                if (inst.type == Type::DECIMAL) printOp = Op::PRINT_D;
                else if (inst.type == Type::TEXT) printOp = Op::PRINT_S;
                else if (inst.type == Type::LETTER) printOp = Op::PRINT_L;
                else if (inst.type == Type::FLAG) printOp = Op::PRINT_F;
                bcFn.code.emplace_back(printOp, inst.line);
                break;
            }
            case Opcode::PRINT_SPACE: {
                bcFn.code.emplace_back(Op::PRINT_SPACE, inst.line);
                break;
            }
            case Opcode::PRINT_NEWLINE: {
                bcFn.code.emplace_back(Op::PRINT_NL, inst.line);
                break;
            }
            case Opcode::INPUT: {
                if (!inst.arg1.isNone()) {
                    pushOperand(inst.arg1, inst.line);
                    bcFn.code.emplace_back(Op::INPUT_PROMPT, inst.line);
                } else {
                    bcFn.code.emplace_back(Op::INPUT, inst.line);
                }
                storeOperand(inst.dest, inst.line);
                break;
            }
        }
    }

    // Always append final RET so execution never runs off the end
    if (bcFn.code.empty() || (bcFn.code.back().op != Op::RET && bcFn.code.back().op != Op::RET_VAL)) {
        bcFn.code.emplace_back(Op::RET, 0);
    }

    // Back-patch all jump instructions
    for (const auto& patch : patches) {
        auto it = labelAddrs.find(patch.labelName);
        if (it != labelAddrs.end()) {
            bcFn.code[patch.instructionIndex].operand = it->second;
        } else {
            // Target was at the very end of instructions
            bcFn.code[patch.instructionIndex].operand = static_cast<int64_t>(bcFn.code.size() - 1);
        }
    }

    bcFn.numSlots = static_cast<int>(bcFn.slotNames.size());
    return bcFn;
}
