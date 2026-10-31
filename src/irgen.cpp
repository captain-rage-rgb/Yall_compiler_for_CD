#include "irgen.h"
#include <iostream>

std::string IRGen::newTemp() {
    tempCounter_++;
    return "t" + std::to_string(tempCounter_);
}

std::string IRGen::newLabel() {
    labelCounter_++;
    return "L" + std::to_string(labelCounter_);
}

void IRGen::emit(Opcode op, Operand dest, Operand arg1, Operand arg2, Type type, int line) {
    if (currentFunc_) {
        currentFunc_->instructions.emplace_back(op, dest, arg1, arg2, type, line);
    }
}

void IRGen::emit(Opcode op, Operand arg1, Type type, int line) {
    emit(op, Operand::none(), arg1, Operand::none(), type, line);
}

void IRGen::emit(Opcode op, Operand arg1, int line) {
    emit(op, Operand::none(), arg1, Operand::none(), Type::NOTHING, line);
}

void IRGen::emit(Opcode op, int line) {
    emit(op, Operand::none(), Operand::none(), Operand::none(), Type::NOTHING, line);
}

Operand IRGen::widenToDecimal(const Operand& op, int line) {
    if (op.type == Type::NUM) {
        std::string t = newTemp();
        Operand dest = Operand::temp(t, Type::DECIMAL);
        emit(Opcode::I2D, dest, op, Operand::none(), Type::DECIMAL, line);
        return dest;
    }
    return op;
}

TACProgram IRGen::generate(ProgramNode* program) {
    TACProgram tac;
    if (!program) return tac;

    // Pass 1: Record all function signatures
    for (const auto& decl : program->declarations) {
        if (decl->kind == NodeKind::FUNC_DECL) {
            auto* fn = static_cast<FuncDecl*>(decl.get());
            FunctionSig sig;
            sig.name = fn->name;
            sig.returnType = fn->resolvedReturnType;
            for (const auto& p : fn->params) {
                sig.paramTypes.push_back(p->resolvedType);
            }
            funcSigs_[fn->name] = sig;
        }
    }

    // Pass 2: Generate TAC for each function
    for (const auto& decl : program->declarations) {
        if (decl->kind == NodeKind::FUNC_DECL) {
            auto* fn = static_cast<FuncDecl*>(decl.get());
            tac.functions.emplace_back();
            currentFunc_ = &tac.functions.back();
            generateFuncDecl(fn);
            currentFunc_ = nullptr;
        }
    }

    return tac;
}

void IRGen::generateFuncDecl(FuncDecl* fn) {
    currentFunc_->name = fn->name;
    currentFunc_->returnType = fn->resolvedReturnType;

    tempCounter_ = 0; // Temp counter restarts per function

    // Register parameter types and symbols
    for (const auto& p : fn->params) {
        currentFunc_->params.push_back({p->uniqueName, p->resolvedType});
        varTypes_[p->uniqueName] = p->resolvedType;
    }

    if (fn->body) {
        generateBlock(fn->body.get());
    }

    // Implicit return for nothing functions or start
    if (currentFunc_->instructions.empty() || currentFunc_->instructions.back().opcode != Opcode::RETURN) {
        emit(Opcode::RETURN, fn->line);
    }
}

void IRGen::generateStatement(Statement* stmt) {
    if (!stmt) return;

    switch (stmt->kind) {
        case NodeKind::BLOCK:
            generateBlock(static_cast<BlockStmt*>(stmt));
            break;
        case NodeKind::VAR_DECL:
            generateVarDecl(static_cast<VarDecl*>(stmt));
            break;
        case NodeKind::ASSIGN_STMT:
            generateAssign(static_cast<AssignStmt*>(stmt));
            break;
        case NodeKind::INC_DEC_STMT:
            generateIncDec(static_cast<IncDecStmt*>(stmt));
            break;
        case NodeKind::OUT_STMT:
            generateOut(static_cast<OutStmt*>(stmt));
            break;
        case NodeKind::IF_STMT:
            generateIf(static_cast<IfStmt*>(stmt));
            break;
        case NodeKind::WHILE_STMT:
            generateWhile(static_cast<WhileStmt*>(stmt));
            break;
        case NodeKind::REP_STMT:
            generateRep(static_cast<RepStmt*>(stmt));
            break;
        case NodeKind::STOP_STMT:
            if (!loopStack_.empty()) {
                emit(Opcode::GOTO, Operand::label(loopStack_.back().breakLabel), stmt->line);
            }
            break;
        case NodeKind::SKIP_STMT:
            if (!loopStack_.empty()) {
                emit(Opcode::GOTO, Operand::label(loopStack_.back().continueLabel), stmt->line);
            }
            break;
        case NodeKind::END_STMT:
            generateEnd(static_cast<EndStmt*>(stmt));
            break;
        case NodeKind::CALL_STMT:
            generateCallStmt(static_cast<CallStmt*>(stmt));
            break;
        default:
            break;
    }
}

void IRGen::generateBlock(BlockStmt* block) {
    if (!block) return;
    for (const auto& stmt : block->statements) {
        generateStatement(stmt.get());
    }
}

void IRGen::generateVarDecl(VarDecl* vd) {
    varTypes_[vd->uniqueName] = vd->resolvedType;
    Operand dest = Operand::var(vd->uniqueName, vd->resolvedType);

    if (vd->initializer) {
        Operand val = generateExpression(vd->initializer.get());
        if (vd->resolvedType == Type::DECIMAL && val.type == Type::NUM) {
            val = widenToDecimal(val, vd->line);
        }
        emit(Opcode::COPY, dest, val, Operand::none(), vd->resolvedType, vd->line);
    } else {
        // Default values
        std::string defStr;
        switch (vd->resolvedType) {
            case Type::NUM: defStr = "0"; break;
            case Type::DECIMAL: defStr = "0.0"; break;
            case Type::TEXT: defStr = ""; break;
            case Type::FLAG: defStr = "no"; break;
            case Type::LETTER: defStr = "\0"; break;
            default: defStr = "0"; break;
        }
        emit(Opcode::COPY, dest, Operand::constant(defStr, vd->resolvedType), Operand::none(), vd->resolvedType, vd->line);
    }
}

void IRGen::generateAssign(AssignStmt* asg) {
    Type tgtType = varTypes_[asg->targetUniqueName];
    Operand dest = Operand::var(asg->targetUniqueName, tgtType);
    Operand val = generateExpression(asg->value.get());

    if (asg->op == "=") {
        if (tgtType == Type::DECIMAL && val.type == Type::NUM) {
            val = widenToDecimal(val, asg->line);
        }
        emit(Opcode::COPY, dest, val, Operand::none(), tgtType, asg->line);
    } else {
        // Compound assignments +=, -=, *=, /=
        if (tgtType == Type::DECIMAL && val.type == Type::NUM) {
            val = widenToDecimal(val, asg->line);
        }
        Opcode op = Opcode::ADD;
        if (asg->op == "+=") op = Opcode::ADD;
        else if (asg->op == "-=") op = Opcode::SUB;
        else if (asg->op == "*=") op = Opcode::MUL;
        else if (asg->op == "/=") op = Opcode::DIV;

        Operand temp = Operand::temp(newTemp(), tgtType);
        emit(op, temp, dest, val, tgtType, asg->line);
        emit(Opcode::COPY, dest, temp, Operand::none(), tgtType, asg->line);
    }
}

void IRGen::generateIncDec(IncDecStmt* id) {
    Operand tgt = Operand::var(id->targetUniqueName, Type::NUM);
    Operand one = Operand::constant("1", Type::NUM);
    Operand temp = Operand::temp(newTemp(), Type::NUM);
    Opcode op = (id->op == "++") ? Opcode::ADD : Opcode::SUB;

    emit(op, temp, tgt, one, Type::NUM, id->line);
    emit(Opcode::COPY, tgt, temp, Operand::none(), Type::NUM, id->line);
}

void IRGen::generateOut(OutStmt* out) {
    if (out->args.empty()) {
        emit(Opcode::PRINT_NEWLINE, out->line);
        return;
    }

    for (size_t i = 0; i < out->args.size(); ++i) {
        Operand val = generateExpression(out->args[i].get());
        if (i > 0) {
            emit(Opcode::PRINT_SPACE, out->line);
        }
        emit(Opcode::PRINT, Operand::none(), val, Operand::none(), val.type, out->line);
    }
    emit(Opcode::PRINT_NEWLINE, out->line);
}

void IRGen::generateIf(IfStmt* ifS) {
    Operand cond = generateExpression(ifS->condition.get());
    std::string lEnd = newLabel();

    if (ifS->elseBranch) {
        std::string lElse = newLabel();
        emit(Opcode::IF_FALSE, Operand::none(), cond, Operand::label(lElse), Type::FLAG, ifS->line);
        generateBlock(ifS->thenBranch.get());
        emit(Opcode::GOTO, Operand::label(lEnd), ifS->line);
        emit(Opcode::LABEL, Operand::label(lElse), ifS->line);

        if (ifS->elseBranch->kind == NodeKind::BLOCK) {
            generateBlock(static_cast<BlockStmt*>(ifS->elseBranch.get()));
        } else if (ifS->elseBranch->kind == NodeKind::IF_STMT) {
            generateIf(static_cast<IfStmt*>(ifS->elseBranch.get()));
        }
        emit(Opcode::LABEL, Operand::label(lEnd), ifS->line);
    } else {
        emit(Opcode::IF_FALSE, Operand::none(), cond, Operand::label(lEnd), Type::FLAG, ifS->line);
        generateBlock(ifS->thenBranch.get());
        emit(Opcode::LABEL, Operand::label(lEnd), ifS->line);
    }
}

void IRGen::generateWhile(WhileStmt* ws) {
    std::string lCond = newLabel();
    std::string lEnd = newLabel();

    loopStack_.push_back({lCond, lEnd});

    emit(Opcode::LABEL, Operand::label(lCond), ws->line);
    Operand cond = generateExpression(ws->condition.get());
    emit(Opcode::IF_FALSE, Operand::none(), cond, Operand::label(lEnd), Type::FLAG, ws->line);

    generateBlock(ws->body.get());

    emit(Opcode::GOTO, Operand::label(lCond), ws->line);
    emit(Opcode::LABEL, Operand::label(lEnd), ws->line);

    loopStack_.pop_back();
}

void IRGen::generateRep(RepStmt* rs) {
    std::string lCond = newLabel();
    std::string lUpdate = newLabel();
    std::string lEnd = newLabel();

    loopStack_.push_back({lUpdate, lEnd});

    if (rs->init) {
        generateStatement(rs->init.get());
    }

    emit(Opcode::LABEL, Operand::label(lCond), rs->line);
    if (rs->condition) {
        Operand cond = generateExpression(rs->condition.get());
        emit(Opcode::IF_FALSE, Operand::none(), cond, Operand::label(lEnd), Type::FLAG, rs->line);
    }

    generateBlock(rs->body.get());

    emit(Opcode::LABEL, Operand::label(lUpdate), rs->line);
    if (rs->update) {
        generateStatement(rs->update.get());
    }

    emit(Opcode::GOTO, Operand::label(lCond), rs->line);
    emit(Opcode::LABEL, Operand::label(lEnd), rs->line);

    loopStack_.pop_back();
}

void IRGen::generateEnd(EndStmt* es) {
    if (es->value) {
        Operand val = generateExpression(es->value.get());
        if (currentFunc_->returnType == Type::DECIMAL && val.type == Type::NUM) {
            val = widenToDecimal(val, es->line);
        }
        emit(Opcode::RETURN, Operand::none(), val, Operand::none(), currentFunc_->returnType, es->line);
    } else {
        emit(Opcode::RETURN, es->line);
    }
}

void IRGen::generateCallStmt(CallStmt* cs) {
    if (!cs->call) return;
    CallExpr* call = cs->call.get();

    FunctionSig sig = funcSigs_[call->callee];
    std::vector<Operand> evalArgs;
    for (size_t i = 0; i < call->args.size(); ++i) {
        Operand arg = generateExpression(call->args[i].get());
        if (i < sig.paramTypes.size() && sig.paramTypes[i] == Type::DECIMAL && arg.type == Type::NUM) {
            arg = widenToDecimal(arg, call->line);
        }
        evalArgs.push_back(arg);
    }

    for (const auto& arg : evalArgs) {
        emit(Opcode::PARAM, Operand::none(), arg, Operand::none(), arg.type, call->line);
    }

    Operand numArgs = Operand::constant(std::to_string(evalArgs.size()), Type::NUM);
    emit(Opcode::CALL, Operand::none(), Operand::func(call->callee), numArgs, Type::NOTHING, call->line);
}

Operand IRGen::generateExpression(Expression* expr) {
    if (!expr) return Operand::none();

    switch (expr->kind) {
        case NodeKind::BINARY_EXPR:
            return generateBinary(static_cast<BinaryExpr*>(expr));
        case NodeKind::UNARY_EXPR:
            return generateUnary(static_cast<UnaryExpr*>(expr));
        case NodeKind::LITERAL_EXPR:
            return generateLiteral(static_cast<LiteralExpr*>(expr));
        case NodeKind::VARIABLE_EXPR:
            return generateVariable(static_cast<VariableExpr*>(expr));
        case NodeKind::CALL_EXPR:
            return generateCallExpr(static_cast<CallExpr*>(expr));
        case NodeKind::IN_EXPR:
            return generateIn(static_cast<InExpr*>(expr));
        default:
            return Operand::none();
    }
}

Operand IRGen::generateBinary(BinaryExpr* bin) {
    // Short-circuit logic for && and ||
    if (bin->op == "&&") {
        std::string tName = newTemp();
        Operand t = Operand::temp(tName, Type::FLAG);
        Operand l = generateExpression(bin->left.get());
        emit(Opcode::COPY, t, l, Operand::none(), Type::FLAG, bin->line);

        std::string lEnd = newLabel();
        emit(Opcode::IF_FALSE, Operand::none(), t, Operand::label(lEnd), Type::FLAG, bin->line);

        Operand r = generateExpression(bin->right.get());
        emit(Opcode::COPY, t, r, Operand::none(), Type::FLAG, bin->line);
        emit(Opcode::LABEL, Operand::label(lEnd), bin->line);
        return t;
    }

    if (bin->op == "||") {
        std::string tName = newTemp();
        Operand t = Operand::temp(tName, Type::FLAG);
        Operand l = generateExpression(bin->left.get());
        emit(Opcode::COPY, t, l, Operand::none(), Type::FLAG, bin->line);

        std::string lEnd = newLabel();
        emit(Opcode::IF_TRUE, Operand::none(), t, Operand::label(lEnd), Type::FLAG, bin->line);

        Operand r = generateExpression(bin->right.get());
        emit(Opcode::COPY, t, r, Operand::none(), Type::FLAG, bin->line);
        emit(Opcode::LABEL, Operand::label(lEnd), bin->line);
        return t;
    }

    Operand l = generateExpression(bin->left.get());
    Operand r = generateExpression(bin->right.get());

    Type opType = Type::NUM;
    Opcode opc = Opcode::ADD;

    if (bin->op == "+" || bin->op == "-" || bin->op == "*" || bin->op == "/") {
        if (l.type == Type::DECIMAL || r.type == Type::DECIMAL) {
            if (l.type == Type::NUM) l = widenToDecimal(l, bin->line);
            if (r.type == Type::NUM) r = widenToDecimal(r, bin->line);
            opType = Type::DECIMAL;
        } else {
            opType = Type::NUM;
        }
        if (bin->op == "+") opc = Opcode::ADD;
        else if (bin->op == "-") opc = Opcode::SUB;
        else if (bin->op == "*") opc = Opcode::MUL;
        else if (bin->op == "/") opc = Opcode::DIV;

        Operand dest = Operand::temp(newTemp(), opType);
        emit(opc, dest, l, r, opType, bin->line);
        return dest;
    }

    if (bin->op == "%") {
        opType = Type::NUM;
        Operand dest = Operand::temp(newTemp(), Type::NUM);
        emit(Opcode::MOD, dest, l, r, Type::NUM, bin->line);
        return dest;
    }

    if (bin->op == "<" || bin->op == "<=" || bin->op == ">" || bin->op == ">=") {
        if (l.type == Type::DECIMAL || r.type == Type::DECIMAL) {
            if (l.type == Type::NUM) l = widenToDecimal(l, bin->line);
            if (r.type == Type::NUM) r = widenToDecimal(r, bin->line);
            opType = Type::DECIMAL;
        } else if (l.type == Type::LETTER) {
            opType = Type::LETTER;
        } else {
            opType = Type::NUM;
        }

        if (bin->op == "<") opc = Opcode::LT;
        else if (bin->op == "<=") opc = Opcode::LE;
        else if (bin->op == ">") opc = Opcode::GT;
        else if (bin->op == ">=") opc = Opcode::GE;

        Operand dest = Operand::temp(newTemp(), Type::FLAG);
        emit(opc, dest, l, r, opType, bin->line);
        return dest;
    }

    if (bin->op == "==" || bin->op == "!=") {
        if (l.type == Type::DECIMAL || r.type == Type::DECIMAL) {
            if (l.type == Type::NUM) l = widenToDecimal(l, bin->line);
            if (r.type == Type::NUM) r = widenToDecimal(r, bin->line);
            opType = Type::DECIMAL;
        } else {
            opType = l.type;
        }

        opc = (bin->op == "==") ? Opcode::EQ : Opcode::NE;
        Operand dest = Operand::temp(newTemp(), Type::FLAG);
        emit(opc, dest, l, r, opType, bin->line);
        return dest;
    }

    return Operand::none();
}

Operand IRGen::generateUnary(UnaryExpr* un) {
    Operand inner = generateExpression(un->operand.get());
    if (un->op == "-") {
        Operand dest = Operand::temp(newTemp(), inner.type);
        emit(Opcode::NEG, dest, inner, Operand::none(), inner.type, un->line);
        return dest;
    }
    if (un->op == "!") {
        Operand dest = Operand::temp(newTemp(), Type::FLAG);
        emit(Opcode::NOT, dest, inner, Operand::none(), Type::FLAG, un->line);
        return dest;
    }
    return Operand::none();
}

Operand IRGen::generateLiteral(LiteralExpr* lit) {
    return Operand::constant(lit->value, lit->type);
}

Operand IRGen::generateVariable(VariableExpr* var) {
    return Operand::var(var->uniqueName, var->type);
}

Operand IRGen::generateCallExpr(CallExpr* call) {
    FunctionSig sig = funcSigs_[call->callee];
    std::vector<Operand> evalArgs;
    for (size_t i = 0; i < call->args.size(); ++i) {
        Operand arg = generateExpression(call->args[i].get());
        if (i < sig.paramTypes.size() && sig.paramTypes[i] == Type::DECIMAL && arg.type == Type::NUM) {
            arg = widenToDecimal(arg, call->line);
        }
        evalArgs.push_back(arg);
    }

    for (const auto& arg : evalArgs) {
        emit(Opcode::PARAM, Operand::none(), arg, Operand::none(), arg.type, call->line);
    }

    Operand numArgs = Operand::constant(std::to_string(evalArgs.size()), Type::NUM);
    if (sig.returnType != Type::NOTHING) {
        Operand dest = Operand::temp(newTemp(), sig.returnType);
        emit(Opcode::CALL, dest, Operand::func(call->callee), numArgs, sig.returnType, call->line);
        return dest;
    } else {
        emit(Opcode::CALL, Operand::none(), Operand::func(call->callee), numArgs, Type::NOTHING, call->line);
        return Operand::none();
    }
}

Operand IRGen::generateIn(InExpr* in) {
    Operand prompt = Operand::none();
    if (in->prompt) {
        prompt = generateExpression(in->prompt.get());
    }
    Operand dest = Operand::temp(newTemp(), Type::TEXT);
    emit(Opcode::INPUT, dest, prompt, Operand::none(), Type::TEXT, in->line);
    return dest;
}
