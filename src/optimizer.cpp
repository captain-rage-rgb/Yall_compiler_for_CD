#include "optimizer.h"
#include "numeric.h"
#include <iomanip>
#include <sstream>
#include <cmath>
#include <unordered_set>

bool TACOptimizer::parseNum(const std::string& str, int64_t& out) {
    try {
        size_t idx = 0;
        if (str.empty()) return false;
        long long val = std::stoll(str, &idx, 10);
        if (idx == str.length()) {
            out = static_cast<int64_t>(val);
            return true;
        }
    } catch (...) {}
    return false;
}

bool TACOptimizer::parseDecimal(const std::string& str, double& out) {
    try {
        size_t idx = 0;
        if (str.empty()) return false;
        double val = std::stod(str, &idx);
        if (idx == str.length()) {
            out = val;
            return true;
        }
    } catch (...) {}
    return false;
}

static std::string doubleToConstString(double d) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%.17g", d);
    return std::string(buf);
}

bool TACOptimizer::isPure(Opcode op, const Instruction& inst) {
    switch (op) {
        case Opcode::ADD:
        case Opcode::SUB:
        case Opcode::MUL:
        case Opcode::NEG:
        case Opcode::LT:
        case Opcode::LE:
        case Opcode::GT:
        case Opcode::GE:
        case Opcode::EQ:
        case Opcode::NE:
        case Opcode::NOT:
        case Opcode::COPY:
        case Opcode::I2D:
            return true;
        case Opcode::DIV:
        case Opcode::MOD: {
            // Rule 2: Pure ONLY when divisor is a nonzero constant
            if (!inst.arg2.isConst()) return false;
            if (inst.type == Type::NUM) {
                int64_t d = 0;
                if (parseNum(inst.arg2.value, d) && d != 0) return true;
            } else if (inst.type == Type::DECIMAL) {
                double d = 0.0;
                if (parseDecimal(inst.arg2.value, d) && d != 0.0) return true;
            }
            return false;
        }
        default:
            return false;
    }
}

std::vector<OptStats> TACOptimizer::optimize(TACProgram& program) {
    std::vector<OptStats> allStats;
    for (auto& fn : program.functions) {
        allStats.push_back(optimizeFunction(fn));
    }
    return allStats;
}

OptStats TACOptimizer::optimizeFunction(TACFunction& fn) {
    OptStats stats;
    stats.functionName = fn.name;
    stats.instructionsBefore = static_cast<int>(fn.instructions.size());

    const int MAX_ITERATIONS = 20;
    for (int iter = 0; iter < MAX_ITERATIONS; ++iter) {
        bool changed = false;
        changed |= passA_ConstantAndCopy(fn, stats);
        changed |= passB_TempForwarding(fn, stats);
        changed |= passC_ControlFlow(fn, stats);
        changed |= passD_DeadCode(fn, stats);
        if (!changed) break;
    }

    stats.instructionsAfter = static_cast<int>(fn.instructions.size());
    return stats;
}

bool TACOptimizer::passA_ConstantAndCopy(TACFunction& fn, OptStats& stats) {
    bool changed = false;
    std::unordered_map<std::string, Operand> known; // operand name -> replacement constant/variable

    auto invalidate = [&](const std::string& name) {
        known.erase(name);
        for (auto it = known.begin(); it != known.end();) {
            if (it->second.value == name) {
                it = known.erase(it);
            } else {
                ++it;
            }
        }
    };

    auto replaceOperand = [&](Operand& op) -> bool {
        if (op.isTemp() || op.isVar()) {
            auto it = known.find(op.value);
            if (it != known.end()) {
                op = it->second;
                stats.propagated++;
                return true;
            }
        }
        return false;
    };

    std::vector<Instruction> newInsts;
    newInsts.reserve(fn.instructions.size());

    for (size_t i = 0; i < fn.instructions.size(); ++i) {
        auto inst = fn.instructions[i];

        // Reset knowledge at labels
        if (inst.opcode == Opcode::LABEL) {
            known.clear();
            newInsts.push_back(inst);
            continue;
        }

        // Replace operand uses
        if (!inst.arg1.isNone()) changed |= replaceOperand(inst.arg1);
        if (!inst.arg2.isNone()) changed |= replaceOperand(inst.arg2);

        // Constant folding for arithmetic / logic
        bool folded = false;

        // Unary NEG
        if (inst.opcode == Opcode::NEG && inst.arg1.isConst()) {
            if (inst.type == Type::NUM) {
                int64_t val = 0;
                if (parseNum(inst.arg1.value, val)) {
                    inst.opcode = Opcode::COPY;
                    inst.arg1 = Operand::constant(std::to_string(wrapNeg(val)), Type::NUM);
                    inst.arg2 = Operand::none();
                    folded = true;
                }
            } else if (inst.type == Type::DECIMAL) {
                double val = 0;
                if (parseDecimal(inst.arg1.value, val)) {
                    inst.opcode = Opcode::COPY;
                    inst.arg1 = Operand::constant(doubleToConstString(-val), Type::DECIMAL);
                    inst.arg2 = Operand::none();
                    folded = true;
                }
            }
        }
        // Unary NOT
        else if (inst.opcode == Opcode::NOT && inst.arg1.isConst()) {
            inst.opcode = Opcode::COPY;
            inst.arg1 = Operand::constant((inst.arg1.value == "yes") ? "no" : "yes", Type::FLAG);
            inst.arg2 = Operand::none();
            folded = true;
        }
        // I2D on constant
        else if (inst.opcode == Opcode::I2D && inst.arg1.isConst()) {
            int64_t val = 0;
            if (parseNum(inst.arg1.value, val)) {
                inst.opcode = Opcode::COPY;
                inst.arg1 = Operand::constant(doubleToConstString(static_cast<double>(val)), Type::DECIMAL);
                inst.arg2 = Operand::none();
                folded = true;
            }
        }
        // Binary constant folding
        else if (inst.arg1.isConst() && inst.arg2.isConst()) {
            if (inst.type == Type::NUM) {
                int64_t a = 0, b = 0;
                if (parseNum(inst.arg1.value, a) && parseNum(inst.arg2.value, b)) {
                    if (inst.opcode == Opcode::ADD) {
                        inst.opcode = Opcode::COPY;
                        inst.arg1 = Operand::constant(std::to_string(wrapAdd(a, b)), Type::NUM);
                        inst.arg2 = Operand::none();
                        folded = true;
                    } else if (inst.opcode == Opcode::SUB) {
                        inst.opcode = Opcode::COPY;
                        inst.arg1 = Operand::constant(std::to_string(wrapSub(a, b)), Type::NUM);
                        inst.arg2 = Operand::none();
                        folded = true;
                    } else if (inst.opcode == Opcode::MUL) {
                        inst.opcode = Opcode::COPY;
                        inst.arg1 = Operand::constant(std::to_string(wrapMul(a, b)), Type::NUM);
                        inst.arg2 = Operand::none();
                        folded = true;
                    } else if (inst.opcode == Opcode::DIV && b != 0) {
                        int64_t res = 0;
                        if (safeDiv(a, b, res)) {
                            inst.opcode = Opcode::COPY;
                            inst.arg1 = Operand::constant(std::to_string(res), Type::NUM);
                            inst.arg2 = Operand::none();
                            folded = true;
                        }
                    } else if (inst.opcode == Opcode::MOD && b != 0) {
                        int64_t res = 0;
                        if (safeMod(a, b, res)) {
                            inst.opcode = Opcode::COPY;
                            inst.arg1 = Operand::constant(std::to_string(res), Type::NUM);
                            inst.arg2 = Operand::none();
                            folded = true;
                        }
                    } else if (inst.opcode == Opcode::LT) {
                        inst.opcode = Opcode::COPY;
                        inst.arg1 = Operand::constant((a < b) ? "yes" : "no", Type::FLAG);
                        inst.arg2 = Operand::none();
                        folded = true;
                    } else if (inst.opcode == Opcode::LE) {
                        inst.opcode = Opcode::COPY;
                        inst.arg1 = Operand::constant((a <= b) ? "yes" : "no", Type::FLAG);
                        inst.arg2 = Operand::none();
                        folded = true;
                    } else if (inst.opcode == Opcode::GT) {
                        inst.opcode = Opcode::COPY;
                        inst.arg1 = Operand::constant((a > b) ? "yes" : "no", Type::FLAG);
                        inst.arg2 = Operand::none();
                        folded = true;
                    } else if (inst.opcode == Opcode::GE) {
                        inst.opcode = Opcode::COPY;
                        inst.arg1 = Operand::constant((a >= b) ? "yes" : "no", Type::FLAG);
                        inst.arg2 = Operand::none();
                        folded = true;
                    } else if (inst.opcode == Opcode::EQ) {
                        inst.opcode = Opcode::COPY;
                        inst.arg1 = Operand::constant((a == b) ? "yes" : "no", Type::FLAG);
                        inst.arg2 = Operand::none();
                        folded = true;
                    } else if (inst.opcode == Opcode::NE) {
                        inst.opcode = Opcode::COPY;
                        inst.arg1 = Operand::constant((a != b) ? "yes" : "no", Type::FLAG);
                        inst.arg2 = Operand::none();
                        folded = true;
                    }
                }
            } else if (inst.type == Type::DECIMAL) {
                double a = 0.0, b = 0.0;
                if (parseDecimal(inst.arg1.value, a) && parseDecimal(inst.arg2.value, b)) {
                    if (inst.opcode == Opcode::ADD) {
                        inst.opcode = Opcode::COPY;
                        inst.arg1 = Operand::constant(doubleToConstString(a + b), Type::DECIMAL);
                        inst.arg2 = Operand::none();
                        folded = true;
                    } else if (inst.opcode == Opcode::SUB) {
                        inst.opcode = Opcode::COPY;
                        inst.arg1 = Operand::constant(doubleToConstString(a - b), Type::DECIMAL);
                        inst.arg2 = Operand::none();
                        folded = true;
                    } else if (inst.opcode == Opcode::MUL) {
                        inst.opcode = Opcode::COPY;
                        inst.arg1 = Operand::constant(doubleToConstString(a * b), Type::DECIMAL);
                        inst.arg2 = Operand::none();
                        folded = true;
                    } else if (inst.opcode == Opcode::DIV && b != 0.0) {
                        // Never fold division by 0.0
                        inst.opcode = Opcode::COPY;
                        inst.arg1 = Operand::constant(doubleToConstString(a / b), Type::DECIMAL);
                        inst.arg2 = Operand::none();
                        folded = true;
                    } else if (inst.opcode == Opcode::LT) {
                        inst.opcode = Opcode::COPY;
                        inst.arg1 = Operand::constant((a < b) ? "yes" : "no", Type::FLAG);
                        inst.arg2 = Operand::none();
                        folded = true;
                    } else if (inst.opcode == Opcode::LE) {
                        inst.opcode = Opcode::COPY;
                        inst.arg1 = Operand::constant((a <= b) ? "yes" : "no", Type::FLAG);
                        inst.arg2 = Operand::none();
                        folded = true;
                    } else if (inst.opcode == Opcode::GT) {
                        inst.opcode = Opcode::COPY;
                        inst.arg1 = Operand::constant((a > b) ? "yes" : "no", Type::FLAG);
                        inst.arg2 = Operand::none();
                        folded = true;
                    } else if (inst.opcode == Opcode::GE) {
                        inst.opcode = Opcode::COPY;
                        inst.arg1 = Operand::constant((a >= b) ? "yes" : "no", Type::FLAG);
                        inst.arg2 = Operand::none();
                        folded = true;
                    } else if (inst.opcode == Opcode::EQ) {
                        inst.opcode = Opcode::COPY;
                        inst.arg1 = Operand::constant((a == b) ? "yes" : "no", Type::FLAG);
                        inst.arg2 = Operand::none();
                        folded = true;
                    } else if (inst.opcode == Opcode::NE) {
                        inst.opcode = Opcode::COPY;
                        inst.arg1 = Operand::constant((a != b) ? "yes" : "no", Type::FLAG);
                        inst.arg2 = Operand::none();
                        folded = true;
                    }
                }
            } else if (inst.type == Type::FLAG) {
                if (inst.opcode == Opcode::EQ) {
                    inst.opcode = Opcode::COPY;
                    inst.arg1 = Operand::constant((inst.arg1.value == inst.arg2.value) ? "yes" : "no", Type::FLAG);
                    inst.arg2 = Operand::none();
                    folded = true;
                } else if (inst.opcode == Opcode::NE) {
                    inst.opcode = Opcode::COPY;
                    inst.arg1 = Operand::constant((inst.arg1.value != inst.arg2.value) ? "yes" : "no", Type::FLAG);
                    inst.arg2 = Operand::none();
                    folded = true;
                }
            } else if (inst.type == Type::LETTER) {
                if (inst.opcode == Opcode::LT) {
                    inst.opcode = Opcode::COPY;
                    inst.arg1 = Operand::constant((inst.arg1.value < inst.arg2.value) ? "yes" : "no", Type::FLAG);
                    inst.arg2 = Operand::none();
                    folded = true;
                } else if (inst.opcode == Opcode::LE) {
                    inst.opcode = Opcode::COPY;
                    inst.arg1 = Operand::constant((inst.arg1.value <= inst.arg2.value) ? "yes" : "no", Type::FLAG);
                    inst.arg2 = Operand::none();
                    folded = true;
                } else if (inst.opcode == Opcode::GT) {
                    inst.opcode = Opcode::COPY;
                    inst.arg1 = Operand::constant((inst.arg1.value > inst.arg2.value) ? "yes" : "no", Type::FLAG);
                    inst.arg2 = Operand::none();
                    folded = true;
                } else if (inst.opcode == Opcode::GE) {
                    inst.opcode = Opcode::COPY;
                    inst.arg1 = Operand::constant((inst.arg1.value >= inst.arg2.value) ? "yes" : "no", Type::FLAG);
                    inst.arg2 = Operand::none();
                    folded = true;
                } else if (inst.opcode == Opcode::EQ) {
                    inst.opcode = Opcode::COPY;
                    inst.arg1 = Operand::constant((inst.arg1.value == inst.arg2.value) ? "yes" : "no", Type::FLAG);
                    inst.arg2 = Operand::none();
                    folded = true;
                } else if (inst.opcode == Opcode::NE) {
                    inst.opcode = Opcode::COPY;
                    inst.arg1 = Operand::constant((inst.arg1.value != inst.arg2.value) ? "yes" : "no", Type::FLAG);
                    inst.arg2 = Operand::none();
                    folded = true;
                }
            }
        }
        // Algebraic identities for num ONLY
        else if (inst.type == Type::NUM) {
            int64_t val1 = 0, val2 = 0;
            bool isConst1 = inst.arg1.isConst() && parseNum(inst.arg1.value, val1);
            bool isConst2 = inst.arg2.isConst() && parseNum(inst.arg2.value, val2);

            if (inst.opcode == Opcode::ADD) {
                if (isConst2 && val2 == 0) { // x + 0
                    inst.opcode = Opcode::COPY;
                    inst.arg2 = Operand::none();
                    folded = true;
                } else if (isConst1 && val1 == 0) { // 0 + x
                    inst.opcode = Opcode::COPY;
                    inst.arg1 = inst.arg2;
                    inst.arg2 = Operand::none();
                    folded = true;
                }
            } else if (inst.opcode == Opcode::SUB) {
                if (isConst2 && val2 == 0) { // x - 0
                    inst.opcode = Opcode::COPY;
                    inst.arg2 = Operand::none();
                    folded = true;
                }
            } else if (inst.opcode == Opcode::MUL) {
                if (isConst2 && val2 == 1) { // x * 1
                    inst.opcode = Opcode::COPY;
                    inst.arg2 = Operand::none();
                    folded = true;
                } else if (isConst1 && val1 == 1) { // 1 * x
                    inst.opcode = Opcode::COPY;
                    inst.arg1 = inst.arg2;
                    inst.arg2 = Operand::none();
                    folded = true;
                } else if ((isConst2 && val2 == 0) || (isConst1 && val1 == 0)) { // x * 0 or 0 * x
                    inst.opcode = Opcode::COPY;
                    inst.arg1 = Operand::constant("0", Type::NUM);
                    inst.arg2 = Operand::none();
                    folded = true;
                }
            } else if (inst.opcode == Opcode::DIV) {
                if (isConst2 && val2 == 1) { // x / 1
                    inst.opcode = Opcode::COPY;
                    inst.arg2 = Operand::none();
                    folded = true;
                }
            }
        }

        if (folded) {
            stats.folded++;
            changed = true;
        }

        // Branch folding
        if (inst.opcode == Opcode::IF_FALSE && inst.arg1.isConst()) {
            if (inst.arg1.value == "yes") {
                // Condition always yes: IF_FALSE never branches, drop it
                stats.folded++;
                stats.removed++;
                changed = true;
                continue;
            } else if (inst.arg1.value == "no") {
                // Condition always no: IF_FALSE always branches -> GOTO
                inst.opcode = Opcode::GOTO;
                inst.arg1 = inst.arg2;
                inst.arg2 = Operand::none();
                stats.folded++;
                changed = true;
            }
        } else if (inst.opcode == Opcode::IF_TRUE && inst.arg1.isConst()) {
            if (inst.arg1.value == "yes") {
                // Condition always yes: IF_TRUE always branches -> GOTO
                inst.opcode = Opcode::GOTO;
                inst.arg1 = inst.arg2;
                inst.arg2 = Operand::none();
                stats.folded++;
                changed = true;
            } else if (inst.arg1.value == "no") {
                // Condition always no: drop it
                stats.folded++;
                stats.removed++;
                changed = true;
                continue;
            }
        }

        // Record COPY knowledge or invalidate on reassignment
        if (!inst.dest.isNone()) {
            invalidate(inst.dest.value);
            if (inst.opcode == Opcode::COPY) {
                if (inst.arg1.isConst() || inst.arg1.isVar() || inst.arg1.isTemp()) {
                    known[inst.dest.value] = inst.arg1;
                }
            }
        }

        newInsts.push_back(inst);
    }

    fn.instructions = std::move(newInsts);
    return changed;
}

bool TACOptimizer::passB_TempForwarding(TACFunction& fn, OptStats& stats) {
    if (fn.instructions.size() < 2) return false;

    // Count definitions and uses of each temp across the function
    std::unordered_map<std::string, int> defCount;
    std::unordered_map<std::string, int> useCount;

    for (const auto& inst : fn.instructions) {
        if (inst.dest.isTemp()) {
            defCount[inst.dest.value]++;
        }
        if (inst.arg1.isTemp()) useCount[inst.arg1.value]++;
        if (inst.arg2.isTemp()) useCount[inst.arg2.value]++;
    }

    bool changed = false;
    for (size_t i = 0; i + 1 < fn.instructions.size(); ++i) {
        auto& first = fn.instructions[i];
        auto& next = fn.instructions[i + 1];

        if (first.dest.isTemp() && next.opcode == Opcode::COPY) {
            std::string t = first.dest.value;
            if (next.arg1.isTemp() && next.arg1.value == t) {
                if (defCount[t] == 1 && useCount[t] == 1) {
                    // Rewrite first destination and delete the COPY
                    first.dest = next.dest;
                    fn.instructions.erase(fn.instructions.begin() + i + 1);
                    stats.forwarded++;
                    stats.removed++;
                    changed = true;
                    break; // Restart analysis after vector modification
                }
            }
        }
    }

    return changed;
}

bool TACOptimizer::passC_ControlFlow(TACFunction& fn, OptStats& stats) {
    bool changed = false;

    // 1. Delete unreachable code after unconditional GOTO or RETURN until next LABEL
    std::vector<Instruction> newInsts;
    bool dead = false;
    for (const auto& inst : fn.instructions) {
        if (inst.opcode == Opcode::LABEL) {
            dead = false;
            newInsts.push_back(inst);
        } else if (dead) {
            stats.removed++;
            changed = true;
        } else {
            newInsts.push_back(inst);
            if (inst.opcode == Opcode::GOTO || inst.opcode == Opcode::RETURN) {
                dead = true;
            }
        }
    }
    fn.instructions = std::move(newInsts);

    static auto getLabelName = [](const Instruction& inst) -> std::string {
        return inst.dest.isNone() ? inst.arg1.value : inst.dest.value;
    };

    // 2. Jump threading: follow label -> GOTO target chains
    std::unordered_map<std::string, std::string> jumpMap;
    for (size_t i = 0; i + 1 < fn.instructions.size(); ++i) {
        if (fn.instructions[i].opcode == Opcode::LABEL &&
            fn.instructions[i + 1].opcode == Opcode::GOTO) {
            jumpMap[getLabelName(fn.instructions[i])] = fn.instructions[i + 1].arg1.value;
        }
    }

    auto resolveTarget = [&](const std::string& target) -> std::string {
        std::string curr = target;
        std::unordered_set<std::string> visited;
        while (jumpMap.find(curr) != jumpMap.end()) {
            if (visited.count(curr)) break; // Prevent cycle
            visited.insert(curr);
            curr = jumpMap[curr];
        }
        return curr;
    };

    for (auto& inst : fn.instructions) {
        if (inst.opcode == Opcode::GOTO) {
            std::string res = resolveTarget(inst.arg1.value);
            if (res != inst.arg1.value) {
                inst.arg1.value = res;
                changed = true;
            }
        } else if (inst.opcode == Opcode::IF_FALSE || inst.opcode == Opcode::IF_TRUE) {
            std::string res = resolveTarget(inst.arg2.value);
            if (res != inst.arg2.value) {
                inst.arg2.value = res;
                changed = true;
            }
        }
    }

    // 3. Delete GOTO L immediately followed by LABEL L:
    for (size_t i = 0; i + 1 < fn.instructions.size();) {
        if (fn.instructions[i].opcode == Opcode::GOTO &&
            fn.instructions[i + 1].opcode == Opcode::LABEL &&
            fn.instructions[i].arg1.value == getLabelName(fn.instructions[i + 1])) {
            fn.instructions.erase(fn.instructions.begin() + i);
            stats.removed++;
            changed = true;
        } else {
            ++i;
        }
    }

    // 4. Delete unreferenced labels
    std::unordered_set<std::string> referencedLabels;
    for (const auto& inst : fn.instructions) {
        if (inst.opcode == Opcode::GOTO) {
            referencedLabels.insert(inst.arg1.value);
        } else if (inst.opcode == Opcode::IF_FALSE || inst.opcode == Opcode::IF_TRUE) {
            referencedLabels.insert(inst.arg2.value);
        }
    }

    std::vector<Instruction> filtered;
    for (const auto& inst : fn.instructions) {
        if (inst.opcode == Opcode::LABEL && !referencedLabels.count(getLabelName(inst))) {
            stats.removed++;
            changed = true;
        } else {
            filtered.push_back(inst);
        }
    }
    fn.instructions = std::move(filtered);

    return changed;
}

bool TACOptimizer::passD_DeadCode(TACFunction& fn, OptStats& stats) {
    bool changed = false;

    // Count uses of every variable and temp
    std::unordered_map<std::string, int> useCount;
    for (const auto& inst : fn.instructions) {
        if (inst.arg1.isTemp() || inst.arg1.isVar()) useCount[inst.arg1.value]++;
        if (inst.arg2.isTemp() || inst.arg2.isVar()) useCount[inst.arg2.value]++;
    }

    std::vector<Instruction> newInsts;
    for (auto& inst : fn.instructions) {
        // Drop CALL dest if unused
        if (inst.opcode == Opcode::CALL && !inst.dest.isNone()) {
            if (useCount[inst.dest.value] == 0) {
                inst.dest = Operand::none();
                changed = true;
            }
            newInsts.push_back(inst);
            continue;
        }

        // Pure instructions with unused destination can be eliminated
        if (!inst.dest.isNone() && isPure(inst.opcode, inst)) {
            if (useCount[inst.dest.value] == 0) {
                stats.removed++;
                changed = true;
                continue;
            }
        }

        newInsts.push_back(inst);
    }

    fn.instructions = std::move(newInsts);
    return changed;
}

void TACOptimizer::printStatsTable(const std::vector<OptStats>& allStats, std::ostream& out) {
    out << "\n"
        << std::left
        << std::setw(20) << "FUNCTION"
        << std::setw(10) << "BEFORE"
        << std::setw(10) << "AFTER"
        << std::setw(10) << "FOLDED"
        << std::setw(14) << "PROPAGATED"
        << std::setw(12) << "FORWARDED"
        << std::setw(10) << "REMOVED"
        << "\n";
    out << std::string(86, '-') << "\n";

    OptStats totals;
    totals.functionName = "TOTAL";

    for (const auto& s : allStats) {
        out << std::left
            << std::setw(20) << s.functionName
            << std::setw(10) << s.instructionsBefore
            << std::setw(10) << s.instructionsAfter
            << std::setw(10) << s.folded
            << std::setw(14) << s.propagated
            << std::setw(12) << s.forwarded
            << std::setw(10) << s.removed
            << "\n";

        totals.instructionsBefore += s.instructionsBefore;
        totals.instructionsAfter += s.instructionsAfter;
        totals.folded += s.folded;
        totals.propagated += s.propagated;
        totals.forwarded += s.forwarded;
        totals.removed += s.removed;
    }

    out << std::string(86, '-') << "\n";
    out << std::left
        << std::setw(20) << totals.functionName
        << std::setw(10) << totals.instructionsBefore
        << std::setw(10) << totals.instructionsAfter
        << std::setw(10) << totals.folded
        << std::setw(14) << totals.propagated
        << std::setw(12) << totals.forwarded
        << std::setw(10) << totals.removed
        << "\n";
}
