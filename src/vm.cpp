#include "vm.h"
#include "numeric.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>

VM::VM(const BCProgram& program) : program_(program) {
    // Pre-populate string table with constants
    for (const auto& c : program_.constants) {
        if (c.kind == ConstEntry::STRING) {
            stringTable_.push_back(c.s);
        }
    }
}

void VM::runtimeError(int line, const std::string& message) {
    std::cout.flush();
    std::cerr << "Runtime error (line " << line << "): " << message << "\n";
    std::cerr.flush();
}

std::string VM::formatDouble(double d) {
    if (std::isnan(d)) return "nan";
    if (std::isinf(d)) return (d < 0) ? "-inf" : "inf";

    // For integer-valued doubles in normal range, format as integer + ".0" (e.g. 100.0 -> "100.0")
    if (d == std::floor(d) && std::abs(d) < 1e15) {
        return std::to_string(static_cast<int64_t>(d)) + ".0";
    }

    char buf[64];
    for (int p = 1; p <= 17; ++p) {
        snprintf(buf, sizeof(buf), "%.*g", p, d);
        char* end = nullptr;
        double parsed = strtod(buf, &end);
        if (end != buf && parsed == d) {
            std::string res = buf;
            if (res.find('.') == std::string::npos &&
                res.find('e') == std::string::npos &&
                res.find('E') == std::string::npos) {
                res += ".0";
            }
            return res;
        }
    }
    snprintf(buf, sizeof(buf), "%.17g", d);
    std::string res = buf;
    if (res.find('.') == std::string::npos &&
        res.find('e') == std::string::npos &&
        res.find('E') == std::string::npos) {
        res += ".0";
    }
    return res;
}

int VM::run(bool trace, int64_t maxSteps) {
    if (program_.entryIndex < 0 || program_.entryIndex >= static_cast<int>(program_.functions.size())) {
        std::cerr << "Runtime error: entry function 'start' not found\n";
        return 3;
    }

    stack_.clear();
    callStack_.clear();

    // Map constant indices to string table handles
    std::vector<int64_t> constStringHandles(program_.constants.size(), 0);
    size_t sIdx = 0;
    for (size_t i = 0; i < program_.constants.size(); ++i) {
        if (program_.constants[i].kind == ConstEntry::STRING) {
            constStringHandles[i] = static_cast<int64_t>(sIdx++);
        }
    }

    const auto& entryFn = program_.functions[program_.entryIndex];
    callStack_.emplace_back(program_.entryIndex, entryFn.numSlots, 0);

    auto push = [&](Cell val) {
        stack_.push_back(val);
    };

    auto pop = [&]() -> Cell {
        if (stack_.empty()) {
            return Cell(int64_t(0));
        }
        Cell val = stack_.back();
        stack_.pop_back();
        return val;
    };

    int64_t stepCount = 0;

    while (!callStack_.empty()) {
        Frame& frame = callStack_.back();
        const auto& fn = program_.functions[frame.funcIndex];

        if (frame.pc >= fn.code.size()) {
            // Implicit return at end of function
            callStack_.pop_back();
            continue;
        }

        const auto& inst = fn.code[frame.pc];

        if (maxSteps > 0 && ++stepCount > maxSteps) {
            runtimeError(inst.line, "step limit exceeded");
            return 3;
        }

        if (trace) {
            std::cerr << "[line " << inst.line << "] " << opToString(inst.op) << " " << inst.operand;
            if (!stack_.empty()) {
                std::cerr << "  (top: " << stack_.back().i << " / " << stack_.back().d << ")";
            }
            std::cerr << "\n";
            std::cerr.flush();
        }

        frame.pc++; // Advance instruction pointer

        switch (inst.op) {
            case Op::PUSH_CONST: {
                if (inst.operand < 0 || inst.operand >= static_cast<int64_t>(program_.constants.size())) {
                    runtimeError(inst.line, "internal VM error: bad constant index");
                    return 3;
                }
                const auto& c = program_.constants[inst.operand];
                if (c.kind == ConstEntry::INT) push(Cell(c.i));
                else if (c.kind == ConstEntry::DOUBLE) push(Cell(c.d));
                else if (c.kind == ConstEntry::STRING) push(Cell(constStringHandles[inst.operand]));
                break;
            }
            case Op::LOAD: {
                if (inst.operand < 0 || inst.operand >= static_cast<int64_t>(frame.locals.size())) {
                    runtimeError(inst.line, "internal VM error: bad slot index");
                    return 3;
                }
                push(frame.locals[inst.operand]);
                break;
            }
            case Op::STORE: {
                if (inst.operand < 0 || inst.operand >= static_cast<int64_t>(frame.locals.size())) {
                    runtimeError(inst.line, "internal VM error: bad slot index");
                    return 3;
                }
                frame.locals[inst.operand] = pop();
                break;
            }
            case Op::POP: {
                pop();
                break;
            }
            case Op::ADD_I: {
                int64_t b = pop().i;
                int64_t a = pop().i;
                push(Cell(wrapAdd(a, b)));
                break;
            }
            case Op::SUB_I: {
                int64_t b = pop().i;
                int64_t a = pop().i;
                push(Cell(wrapSub(a, b)));
                break;
            }
            case Op::MUL_I: {
                int64_t b = pop().i;
                int64_t a = pop().i;
                push(Cell(wrapMul(a, b)));
                break;
            }
            case Op::DIV_I: {
                int64_t b = pop().i;
                int64_t a = pop().i;
                int64_t res = 0;
                if (!safeDiv(a, b, res)) {
                    runtimeError(inst.line, "division by zero");
                    return 3;
                }
                push(Cell(res));
                break;
            }
            case Op::MOD_I: {
                int64_t b = pop().i;
                int64_t a = pop().i;
                int64_t res = 0;
                if (!safeMod(a, b, res)) {
                    runtimeError(inst.line, "division by zero");
                    return 3;
                }
                push(Cell(res));
                break;
            }
            case Op::NEG_I: {
                int64_t a = pop().i;
                push(Cell(wrapNeg(a)));
                break;
            }
            case Op::ADD_D: {
                double b = pop().d;
                double a = pop().d;
                push(Cell(a + b));
                break;
            }
            case Op::SUB_D: {
                double b = pop().d;
                double a = pop().d;
                push(Cell(a - b));
                break;
            }
            case Op::MUL_D: {
                double b = pop().d;
                double a = pop().d;
                push(Cell(a * b));
                break;
            }
            case Op::DIV_D: {
                double b = pop().d;
                double a = pop().d;
                if (b == 0.0) {
                    runtimeError(inst.line, "division by zero");
                    return 3;
                }
                push(Cell(a / b));
                break;
            }
            case Op::NEG_D: {
                double a = pop().d;
                push(Cell(-a));
                break;
            }
            case Op::I2D: {
                int64_t a = pop().i;
                push(Cell(static_cast<double>(a)));
                break;
            }
            case Op::LT_I: {
                int64_t b = pop().i;
                int64_t a = pop().i;
                push(Cell((a < b) ? 1LL : 0LL));
                break;
            }
            case Op::LE_I: {
                int64_t b = pop().i;
                int64_t a = pop().i;
                push(Cell((a <= b) ? 1LL : 0LL));
                break;
            }
            case Op::GT_I: {
                int64_t b = pop().i;
                int64_t a = pop().i;
                push(Cell((a > b) ? 1LL : 0LL));
                break;
            }
            case Op::GE_I: {
                int64_t b = pop().i;
                int64_t a = pop().i;
                push(Cell((a >= b) ? 1LL : 0LL));
                break;
            }
            case Op::EQ_I: {
                int64_t b = pop().i;
                int64_t a = pop().i;
                push(Cell((a == b) ? 1LL : 0LL));
                break;
            }
            case Op::NE_I: {
                int64_t b = pop().i;
                int64_t a = pop().i;
                push(Cell((a != b) ? 1LL : 0LL));
                break;
            }
            case Op::LT_D: {
                double b = pop().d;
                double a = pop().d;
                push(Cell((a < b) ? 1LL : 0LL));
                break;
            }
            case Op::LE_D: {
                double b = pop().d;
                double a = pop().d;
                push(Cell((a <= b) ? 1LL : 0LL));
                break;
            }
            case Op::GT_D: {
                double b = pop().d;
                double a = pop().d;
                push(Cell((a > b) ? 1LL : 0LL));
                break;
            }
            case Op::GE_D: {
                double b = pop().d;
                double a = pop().d;
                push(Cell((a >= b) ? 1LL : 0LL));
                break;
            }
            case Op::EQ_D: {
                double b = pop().d;
                double a = pop().d;
                push(Cell((a == b) ? 1LL : 0LL));
                break;
            }
            case Op::NE_D: {
                double b = pop().d;
                double a = pop().d;
                push(Cell((a != b) ? 1LL : 0LL));
                break;
            }
            case Op::EQ_S: {
                int64_t b = pop().i;
                int64_t a = pop().i;
                bool eq = (a >= 0 && a < static_cast<int64_t>(stringTable_.size()) &&
                           b >= 0 && b < static_cast<int64_t>(stringTable_.size()) &&
                           stringTable_[a] == stringTable_[b]);
                push(Cell(eq ? 1LL : 0LL));
                break;
            }
            case Op::NE_S: {
                int64_t b = pop().i;
                int64_t a = pop().i;
                bool ne = (a >= 0 && a < static_cast<int64_t>(stringTable_.size()) &&
                           b >= 0 && b < static_cast<int64_t>(stringTable_.size()) &&
                           stringTable_[a] != stringTable_[b]);
                push(Cell(ne ? 1LL : 0LL));
                break;
            }
            case Op::NOT: {
                int64_t a = pop().i;
                push(Cell((a == 0) ? 1LL : 0LL));
                break;
            }
            case Op::JMP: {
                frame.pc = inst.operand;
                break;
            }
            case Op::JMP_FALSE: {
                int64_t cond = pop().i;
                if (cond == 0) {
                    frame.pc = inst.operand;
                }
                break;
            }
            case Op::JMP_TRUE: {
                int64_t cond = pop().i;
                if (cond != 0) {
                    frame.pc = inst.operand;
                }
                break;
            }
            case Op::CALL: {
                int fIdx = static_cast<int>(inst.operand);
                if (fIdx < 0 || fIdx >= static_cast<int>(program_.functions.size())) {
                    runtimeError(inst.line, "internal VM error: bad function index");
                    return 3;
                }
                if (callStack_.size() >= MAX_CALL_DEPTH) {
                    runtimeError(inst.line, "stack overflow (recursion too deep)");
                    return 3;
                }

                const auto& callee = program_.functions[fIdx];
                Frame newFrame(fIdx, callee.numSlots, stack_.size() - callee.numParams);
                for (int p = callee.numParams - 1; p >= 0; --p) {
                    newFrame.locals[p] = pop();
                }
                callStack_.push_back(std::move(newFrame));
                break;
            }
            case Op::RET: {
                callStack_.pop_back();
                if (callStack_.empty()) {
                    std::cout.flush();
                    return 0;
                }
                break;
            }
            case Op::RET_VAL: {
                Cell res = pop();
                callStack_.pop_back();
                if (callStack_.empty()) {
                    std::cout.flush();
                    return 0;
                }
                push(res);
                break;
            }
            case Op::PRINT_I: {
                std::cout << pop().i;
                break;
            }
            case Op::PRINT_D: {
                std::cout << formatDouble(pop().d);
                break;
            }
            case Op::PRINT_S: {
                int64_t handle = pop().i;
                if (handle >= 0 && handle < static_cast<int64_t>(stringTable_.size())) {
                    std::cout << stringTable_[handle];
                }
                break;
            }
            case Op::PRINT_L: {
                std::cout << static_cast<char>(pop().i);
                break;
            }
            case Op::PRINT_F: {
                std::cout << ((pop().i != 0) ? "yes" : "no");
                break;
            }
            case Op::PRINT_SPACE: {
                std::cout << " ";
                break;
            }
            case Op::PRINT_NL: {
                std::cout << "\n";
                break;
            }
            case Op::INPUT: {
                std::cout.flush();
                std::string line;
                if (std::getline(std::cin, line)) {
                    if (!line.empty() && line.back() == '\r') {
                        line.pop_back();
                    }
                } else {
                    line = "";
                }
                int64_t handle = static_cast<int64_t>(stringTable_.size());
                stringTable_.push_back(line);
                push(Cell(handle));
                break;
            }
            case Op::INPUT_PROMPT: {
                int64_t pHandle = pop().i;
                if (pHandle >= 0 && pHandle < static_cast<int64_t>(stringTable_.size())) {
                    std::cout << stringTable_[pHandle];
                }
                std::cout.flush();
                std::string line;
                if (std::getline(std::cin, line)) {
                    if (!line.empty() && line.back() == '\r') {
                        line.pop_back();
                    }
                } else {
                    line = "";
                }
                int64_t handle = static_cast<int64_t>(stringTable_.size());
                stringTable_.push_back(line);
                push(Cell(handle));
                break;
            }
            case Op::HALT: {
                std::cout.flush();
                return 0;
            }
        }
    }

    std::cout.flush();
    return 0;
}
