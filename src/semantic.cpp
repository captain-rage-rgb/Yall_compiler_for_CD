#include "semantic.h"
#include <algorithm>
#include <climits>

SemanticAnalyzer::SemanticAnalyzer() {}

const std::vector<SemanticError>& SemanticAnalyzer::errors() const {
    return errors_;
}

const std::vector<SemanticWarning>& SemanticAnalyzer::warnings() const {
    return warnings_;
}

const SymbolTable& SemanticAnalyzer::symbolTable() const {
    return symTab_;
}

void SemanticAnalyzer::addError(int line, int col, const std::string& message) {
    // Suppress duplicate identical errors at the same position
    if (!errors_.empty() && errors_.back().line == line &&
        errors_.back().column == col && errors_.back().message == message) {
        return;
    }
    errors_.push_back(SemanticError{line, col, message});
}

void SemanticAnalyzer::addWarning(int line, int col, const std::string& message) {
    if (!warnings_.empty() && warnings_.back().line == line &&
        warnings_.back().column == col && warnings_.back().message == message) {
        return;
    }
    warnings_.push_back(SemanticWarning{line, col, message});
}

// ============================================================================
// Two-Pass Analysis Coordinator
// ============================================================================
void SemanticAnalyzer::analyze(ProgramNode* program) {
    if (!program) return;

    // Pass 1: Record all function signatures in the global scope
    pass1CollectFunctions(program);

    // Pass 2: Analyze function bodies and statements
    pass2AnalyzeProgram(program);

    // Collect unused variable warnings from the symbol table
    for (const auto& u : symTab_.getUnusedVariables()) {
        addWarning(u.line, u.col, "variable '" + u.name + "' is declared but never used");
    }

    // Sort errors and warnings by source line and column
    std::sort(errors_.begin(), errors_.end(), [](const SemanticError& a, const SemanticError& b) {
        if (a.line != b.line) return a.line < b.line;
        return a.column < b.column;
    });

    std::sort(warnings_.begin(), warnings_.end(), [](const SemanticWarning& a, const SemanticWarning& b) {
        if (a.line != b.line) return a.line < b.line;
        return a.column < b.column;
    });
}

// ============================================================================
// Pass 1: Top-Level Function Signature Collection
// ============================================================================
void SemanticAnalyzer::pass1CollectFunctions(ProgramNode* program) {
    int startCount = 0;

    for (const auto& decl : program->declarations) {
        // Rule 3: Any top-level item that is not a function declaration is an error
        if (decl->kind != NodeKind::FUNC_DECL) {
            addError(decl->line, decl->column, "statements must be inside a function");
            continue;
        }

        auto* fn = static_cast<FuncDecl*>(decl.get());

        // Resolve return type
        if (fn->name == "start") {
            startCount++;
            fn->resolvedReturnType = Type::NOTHING;
            if (startCount > 1) {
                // Rule 1: Duplicate start function
                addError(fn->line, fn->column, "duplicate function 'start'");
            }
        } else {
            fn->resolvedReturnType = stringToType(fn->returnType);
        }

        // Collect parameter types
        std::vector<Type> pTypes;
        std::vector<std::string> pNames;
        for (const auto& p : fn->params) {
            Type pt = stringToType(p->typeName);
            p->resolvedType = pt;
            pTypes.push_back(pt);
            pNames.push_back(p->name);
        }

        // Register in symbol table
        bool ok = symTab_.declareFunction(fn->name, fn->resolvedReturnType, pTypes, pNames, fn->line, fn->column);
        if (!ok && fn->name != "start") {
            // Rule 2: Duplicate function names are errors
            addError(fn->line, fn->column, "duplicate function '" + fn->name + "'");
        }
    }

    // Rule 1: There must be exactly one "func start()"
    if (startCount == 0) {
        addError(1, 1, "program has no 'start' function");
    }
}

// ============================================================================
// Pass 2: Program and Function Body Analysis
// ============================================================================
void SemanticAnalyzer::pass2AnalyzeProgram(ProgramNode* program) {
    for (const auto& decl : program->declarations) {
        if (decl->kind == NodeKind::FUNC_DECL) {
            analyzeFunction(static_cast<FuncDecl*>(decl.get()));
        } else {
            // Analyze the top-level statement anyway to catch expression errors
            analyzeStatement(decl.get());
        }
    }
}

void SemanticAnalyzer::analyzeFunction(FuncDecl* fn) {
    currentFunctionReturnType_ = fn->resolvedReturnType;
    currentFunctionName_ = fn->name;
    loopDepth_ = 0;

    // Enter function scope
    symTab_.enterScope("Function " + fn->name + " (Line " + std::to_string(fn->line) + ")");

    // Bind parameters into the function scope
    for (auto& p : fn->params) {
        std::string uName;
        bool ok = symTab_.declareVariable(p->name, p->resolvedType, false,
                                          p->line, p->column, SymbolKind::PARAMETER, uName);
        p->uniqueName = uName;
        if (!ok) {
            addError(p->line, p->column, "duplicate parameter name '" + p->name + "'");
        }
    }

    // Analyze outermost body block sharing the function's scope
    if (fn->body) {
        analyzeBlock(fn->body.get(), /*createScope=*/false);
    }

    symTab_.exitScope();

    // Rule 27: Function with non-nothing return type must return on all paths
    if (fn->resolvedReturnType != Type::NOTHING) {
        if (!alwaysReturns(fn->body.get())) {
            addError(fn->line, fn->column, "function '" + fn->name + "' may reach its end without returning a value");
        }
    }
}

// ============================================================================
// Statement Analysis
// ============================================================================
void SemanticAnalyzer::analyzeBlock(BlockStmt* block, bool createScope, const std::string& scopeName) {
    if (!block) return;

    if (createScope) {
        symTab_.enterScope(scopeName);
    }

    bool hasTerminated = false;
    for (const auto& stmt : block->statements) {
        // Rule 30: Unreachable code warning
        if (hasTerminated) {
            addWarning(stmt->line, stmt->column, "unreachable code");
        }

        analyzeStatement(stmt.get());

        if (stmt->kind == NodeKind::END_STMT ||
            stmt->kind == NodeKind::STOP_STMT ||
            stmt->kind == NodeKind::SKIP_STMT) {
            hasTerminated = true;
        }
    }

    if (createScope) {
        symTab_.exitScope();
    }
}

void SemanticAnalyzer::analyzeStatement(Statement* stmt) {
    if (!stmt) return;

    switch (stmt->kind) {
        case NodeKind::VAR_DECL:
            analyzeVarDecl(static_cast<VarDecl*>(stmt));
            break;
        case NodeKind::ASSIGN_STMT:
            analyzeAssign(static_cast<AssignStmt*>(stmt));
            break;
        case NodeKind::INC_DEC_STMT:
            analyzeIncDec(static_cast<IncDecStmt*>(stmt));
            break;
        case NodeKind::OUT_STMT:
            analyzeOut(static_cast<OutStmt*>(stmt));
            break;
        case NodeKind::IF_STMT:
            analyzeIf(static_cast<IfStmt*>(stmt));
            break;
        case NodeKind::WHILE_STMT:
            analyzeWhile(static_cast<WhileStmt*>(stmt));
            break;
        case NodeKind::REP_STMT:
            analyzeRep(static_cast<RepStmt*>(stmt));
            break;
        case NodeKind::STOP_STMT:
            // Rule 28: stop outside loop
            if (loopDepth_ == 0) {
                addError(stmt->line, stmt->column, "'stop' used outside a loop");
            }
            break;
        case NodeKind::SKIP_STMT:
            // Rule 28: skip outside loop
            if (loopDepth_ == 0) {
                addError(stmt->line, stmt->column, "'skip' used outside a loop");
            }
            break;
        case NodeKind::END_STMT:
            analyzeEnd(static_cast<EndStmt*>(stmt));
            break;
        case NodeKind::CALL_STMT:
            analyzeCallStmt(static_cast<CallStmt*>(stmt));
            break;
        case NodeKind::BLOCK:
            analyzeBlock(static_cast<BlockStmt*>(stmt), true, "Block (Line " + std::to_string(stmt->line) + ")");
            break;
        default:
            break;
    }
}

void SemanticAnalyzer::analyzeVarDecl(VarDecl* vd) {
    // Rule 4: "let x = expr;"
    if (vd->typeName == "let") {
        if (!vd->initializer) {
            addError(vd->line, vd->column, "variable '" + vd->name + "' declared with 'let' must have an initializer");
            vd->resolvedType = Type::ERROR;
        } else {
            Type initType = analyzeExpression(vd->initializer.get());
            if (initType == Type::NOTHING) {
                addError(vd->initializer->line, vd->initializer->column,
                         "cannot infer a type from a call to a function that returns nothing");
                vd->resolvedType = Type::ERROR;
            } else {
                vd->resolvedType = initType;
            }
        }
    } else {
        // Explicit type: Rule 5, 6, 7, 8
        Type declType = stringToType(vd->typeName);
        vd->resolvedType = declType;

        if (vd->initializer) {
            Type initType = analyzeExpression(vd->initializer.get());
            if (declType == Type::NUM && initType == Type::DECIMAL) {
                addError(vd->initializer->line, vd->initializer->column,
                         "cannot convert decimal to num without loss of data");
            } else if (!isAssignable(declType, initType)) {
                addError(vd->initializer->line, vd->initializer->column,
                         "cannot assign " + typeToString(initType) + " to variable '" + vd->name + "' of type " + typeToString(declType));
            }
        } else {
            // Rule 8: fixed declarations MUST have an initializer
            if (vd->isFixed) {
                addError(vd->line, vd->column, "fixed variable '" + vd->name + "' must have an initializer");
            }
        }
    }

    // Register in current scope
    std::string uName;
    bool ok = symTab_.declareVariable(vd->name, vd->resolvedType, vd->isFixed,
                                      vd->line, vd->column, SymbolKind::VARIABLE, uName);
    vd->uniqueName = uName;
    if (!ok) {
        addError(vd->line, vd->column, "redeclaration of variable '" + vd->name + "'");
    }
}

void SemanticAnalyzer::analyzeAssign(AssignStmt* asg) {
    // Rule 9, 21: x = expr
    Symbol* sym = symTab_.lookupVariable(asg->target);
    if (!sym) {
        addError(asg->line, asg->column, "undeclared variable '" + asg->target + "'");
        analyzeExpression(asg->value.get());
        return;
    }

    asg->targetUniqueName = sym->uniqueName;
    sym->written = true;

    // Rule 8: Cannot modify fixed variable
    if (sym->isFixed) {
        addError(asg->line, asg->column, "cannot modify fixed variable '" + asg->target + "'");
    }

    Type valType = analyzeExpression(asg->value.get());

    if (asg->op == "=") {
        if (sym->type == Type::NUM && valType == Type::DECIMAL) {
            addError(asg->value->line, asg->value->column, "cannot convert decimal to num without loss of data");
        } else if (!isAssignable(sym->type, valType)) {
            addError(asg->value->line, asg->value->column,
                     "cannot assign " + typeToString(valType) + " to variable '" + asg->target + "' of type " + typeToString(sym->type));
        }
    } else {
        // Rule 19: Compound assignment (+= -= *= /=)
        if (!isNumeric(sym->type) || !isNumeric(valType)) {
            addError(asg->line, asg->column, "operands of '" + asg->op + "' must be numeric");
        } else {
            Type resType = (sym->type == Type::DECIMAL || valType == Type::DECIMAL) ? Type::DECIMAL : Type::NUM;
            if (sym->type == Type::NUM && resType == Type::DECIMAL) {
                addError(asg->value->line, asg->value->column, "cannot convert decimal to num without loss of data");
            }
        }
    }
}

void SemanticAnalyzer::analyzeIncDec(IncDecStmt* id) {
    // Rule 8, 20: ++ and --
    Symbol* sym = symTab_.lookupVariable(id->name);
    if (!sym) {
        addError(id->line, id->column, "undeclared variable '" + id->name + "'");
        return;
    }

    id->targetUniqueName = sym->uniqueName;
    sym->written = true;
    sym->used = true; // Mutating variable counts as read and written

    if (sym->isFixed) {
        addError(id->line, id->column, "cannot modify fixed variable '" + id->name + "'");
    }

    if (sym->type != Type::NUM && sym->type != Type::ERROR) {
        addError(id->line, id->column, "target of '" + id->op + "' must be num, found " + typeToString(sym->type));
    }
}

void SemanticAnalyzer::analyzeOut(OutStmt* out) {
    // Rule 22: every argument must have a type other than nothing
    for (const auto& arg : out->args) {
        Type t = analyzeExpression(arg.get());
        if (t == Type::NOTHING) {
            addError(arg->line, arg->column, "cannot output nothing");
        }
    }
}

void SemanticAnalyzer::analyzeIf(IfStmt* ifS) {
    // Rule 17: Condition must be a flag
    Type condType = analyzeExpression(ifS->condition.get());
    if (condType != Type::FLAG && condType != Type::ERROR) {
        addError(ifS->condition->line, ifS->condition->column,
                 "condition must be a flag, found " + typeToString(condType));
    }

    if (ifS->thenBranch) {
        analyzeBlock(ifS->thenBranch.get(), true, "If Then (Line " + std::to_string(ifS->line) + ")");
    }

    if (ifS->elseBranch) {
        if (ifS->elseBranch->kind == NodeKind::BLOCK) {
            analyzeBlock(static_cast<BlockStmt*>(ifS->elseBranch.get()), true,
                         "If Else (Line " + std::to_string(ifS->line) + ")");
        } else if (ifS->elseBranch->kind == NodeKind::IF_STMT) {
            analyzeIf(static_cast<IfStmt*>(ifS->elseBranch.get()));
        }
    }
}

void SemanticAnalyzer::analyzeWhile(WhileStmt* ws) {
    // Rule 17: Condition must be a flag
    Type condType = analyzeExpression(ws->condition.get());
    if (condType != Type::FLAG && condType != Type::ERROR) {
        addError(ws->condition->line, ws->condition->column,
                 "condition must be a flag, found " + typeToString(condType));
    }

    loopDepth_++;
    analyzeBlock(ws->body.get(), true, "While Body (Line " + std::to_string(ws->line) + ")");
    loopDepth_--;
}

void SemanticAnalyzer::analyzeRep(RepStmt* rs) {
    // Scope for rep header
    symTab_.enterScope("Rep Header (Line " + std::to_string(rs->line) + ")");

    if (rs->init) {
        analyzeStatement(rs->init.get());
    }

    // Rule 17: Loop condition must be a flag
    if (rs->condition) {
        Type condType = analyzeExpression(rs->condition.get());
        if (condType != Type::FLAG && condType != Type::ERROR) {
            addError(rs->condition->line, rs->condition->column,
                     "condition must be a flag, found " + typeToString(condType));
        }
    }

    if (rs->update) {
        analyzeStatement(rs->update.get());
    }

    loopDepth_++;
    analyzeBlock(rs->body.get(), true, "Rep Body (Line " + std::to_string(rs->line) + ")");
    loopDepth_--;

    symTab_.exitScope();
}

void SemanticAnalyzer::analyzeEnd(EndStmt* es) {
    // Rule 26: Return statement handling
    if (currentFunctionReturnType_ == Type::NOTHING) {
        if (es->value) {
            addError(es->line, es->column, "cannot return a value from function with return type nothing");
            analyzeExpression(es->value.get());
        }
    } else {
        if (!es->value) {
            addError(es->line, es->column,
                     "function with return type " + typeToString(currentFunctionReturnType_) + " must return a value");
        } else {
            Type valType = analyzeExpression(es->value.get());
            if (currentFunctionReturnType_ == Type::NUM && valType == Type::DECIMAL) {
                addError(es->value->line, es->value->column, "cannot convert decimal to num without loss of data");
            } else if (!isAssignable(currentFunctionReturnType_, valType)) {
                addError(es->value->line, es->value->column,
                         "cannot return " + typeToString(valType) + " from function with return type " + typeToString(currentFunctionReturnType_));
            }
        }
    }
}

void SemanticAnalyzer::analyzeCallStmt(CallStmt* cs) {
    // Rule 25: Call statements discard return value
    if (cs->call) {
        analyzeCall(cs->call.get(), /*isStatementContext=*/true);
    }
}

// ============================================================================
// Expression Analysis & Type Inference
// ============================================================================
Type SemanticAnalyzer::analyzeExpression(Expression* expr) {
    if (!expr) return Type::ERROR;

    switch (expr->kind) {
        case NodeKind::BINARY_EXPR:
            return analyzeBinary(static_cast<BinaryExpr*>(expr));
        case NodeKind::UNARY_EXPR:
            return analyzeUnary(static_cast<UnaryExpr*>(expr));
        case NodeKind::LITERAL_EXPR:
            return analyzeLiteral(static_cast<LiteralExpr*>(expr));
        case NodeKind::VARIABLE_EXPR:
            return analyzeVariable(static_cast<VariableExpr*>(expr));
        case NodeKind::CALL_EXPR:
            return analyzeCall(static_cast<CallExpr*>(expr), /*isStatementContext=*/false);
        case NodeKind::IN_EXPR:
            return analyzeIn(static_cast<InExpr*>(expr));
        default:
            expr->type = Type::ERROR;
            return Type::ERROR;
    }
}

Type SemanticAnalyzer::analyzeBinary(BinaryExpr* bin) {
    Type lt = analyzeExpression(bin->left.get());
    Type rt = analyzeExpression(bin->right.get());

    if (lt == Type::ERROR || rt == Type::ERROR) {
        bin->type = Type::ERROR;
        return Type::ERROR;
    }

    // Rule 18: Division by zero literal check
    if (bin->op == "/" || bin->op == "%") {
        if (bin->right->kind == NodeKind::LITERAL_EXPR) {
            auto* rLit = static_cast<LiteralExpr*>(bin->right.get());
            if (rLit->value == "0" || rLit->value == "0.0") {
                addError(bin->right->line, bin->right->column, "division by zero");
            }
        }
    }

    // Rule 11: Arithmetic + - * /
    if (bin->op == "+") {
        if (lt == Type::TEXT && rt == Type::TEXT) {
            addError(bin->line, bin->column, "text concatenation is not supported");
            bin->type = Type::ERROR;
            return Type::ERROR;
        }
    }

    if (bin->op == "+" || bin->op == "-" || bin->op == "*" || bin->op == "/") {
        if (!isNumeric(lt) || !isNumeric(rt)) {
            addError(bin->line, bin->column, "operands of '" + bin->op + "' must be numeric");
            bin->type = Type::ERROR;
            return Type::ERROR;
        }
        bin->type = (lt == Type::DECIMAL || rt == Type::DECIMAL) ? Type::DECIMAL : Type::NUM;
        return bin->type;
    }

    // Rule 12: Modulo %
    if (bin->op == "%") {
        if (lt != Type::NUM || rt != Type::NUM) {
            addError(bin->line, bin->column, "operands of '%' must be num");
            bin->type = Type::ERROR;
            return Type::ERROR;
        }
        bin->type = Type::NUM;
        return Type::NUM;
    }

    // Rule 13: Relational comparisons < > <= >=
    if (bin->op == "<" || bin->op == ">" || bin->op == "<=" || bin->op == ">=") {
        if ((isNumeric(lt) && isNumeric(rt)) || (lt == Type::LETTER && rt == Type::LETTER)) {
            bin->type = Type::FLAG;
            return Type::FLAG;
        }
        addError(bin->line, bin->column, "operands of '" + bin->op + "' must be numeric or letter");
        bin->type = Type::FLAG;
        return Type::FLAG;
    }

    // Rule 14: Equality comparisons == !=
    if (bin->op == "==" || bin->op == "!=") {
        if ((isNumeric(lt) && isNumeric(rt)) || (lt == rt && lt != Type::NOTHING)) {
            bin->type = Type::FLAG;
            return Type::FLAG;
        }
        addError(bin->line, bin->column, "cannot compare " + typeToString(lt) + " with " + typeToString(rt));
        bin->type = Type::FLAG;
        return Type::FLAG;
    }

    // Rule 15: Logical operators && ||
    if (bin->op == "&&" || bin->op == "||") {
        if (lt != Type::FLAG || rt != Type::FLAG) {
            addError(bin->line, bin->column, "operands of '" + bin->op + "' must be flag");
        }
        bin->type = Type::FLAG;
        return Type::FLAG;
    }

    bin->type = Type::ERROR;
    return Type::ERROR;
}

Type SemanticAnalyzer::analyzeUnary(UnaryExpr* un) {
    Type ot = analyzeExpression(un->operand.get());

    if (ot == Type::ERROR) {
        un->type = Type::ERROR;
        return Type::ERROR;
    }

    // Rule 16: Unary minus
    if (un->op == "-") {
        if (!isNumeric(ot)) {
            addError(un->line, un->column, "operand of unary minus must be numeric");
            un->type = Type::ERROR;
            return Type::ERROR;
        }
        un->type = ot;
        return ot;
    }

    // Rule 15: Logical negation !
    if (un->op == "!") {
        if (ot != Type::FLAG) {
            addError(un->line, un->column, "operand of '!' must be flag");
        }
        un->type = Type::FLAG;
        return Type::FLAG;
    }

    un->type = Type::ERROR;
    return Type::ERROR;
}

Type SemanticAnalyzer::analyzeLiteral(LiteralExpr* lit) {
    if (lit->literalType == "num") {
        // Rule 10: 64-bit integer overflow check
        try {
            size_t idx = 0;
            unsigned long long val = std::stoull(lit->value, &idx, 10);
            if (idx != lit->value.length() || val > 9223372036854775807ULL) {
                addError(lit->line, lit->column, "integer literal out of 64-bit signed range");
                lit->type = Type::ERROR;
                return Type::ERROR;
            }
        } catch (...) {
            addError(lit->line, lit->column, "integer literal out of 64-bit signed range");
            lit->type = Type::ERROR;
            return Type::ERROR;
        }
        lit->type = Type::NUM;
        return Type::NUM;
    }

    if (lit->literalType == "decimal") {
        lit->type = Type::DECIMAL;
        return Type::DECIMAL;
    }

    if (lit->literalType == "text") {
        lit->type = Type::TEXT;
        return Type::TEXT;
    }

    if (lit->literalType == "letter") {
        lit->type = Type::LETTER;
        return Type::LETTER;
    }

    if (lit->literalType == "flag") {
        lit->type = Type::FLAG;
        return Type::FLAG;
    }

    lit->type = Type::ERROR;
    return Type::ERROR;
}

Type SemanticAnalyzer::analyzeVariable(VariableExpr* var) {
    // Rule 9: Undeclared variable lookup
    Symbol* sym = symTab_.lookupVariable(var->name);
    if (!sym) {
        addError(var->line, var->column, "undeclared variable '" + var->name + "'");
        var->type = Type::ERROR;
        return Type::ERROR;
    }

    sym->used = true; // Mark as read
    var->uniqueName = sym->uniqueName;
    var->type = sym->type;
    return var->type;
}

Type SemanticAnalyzer::analyzeCall(CallExpr* call, bool isStatementContext) {
    // Rule 24: Function lookup
    Symbol* fnSym = symTab_.lookupFunction(call->callee);
    if (!fnSym) {
        addError(call->line, call->column, "undeclared function '" + call->callee + "'");
        for (const auto& arg : call->args) {
            analyzeExpression(arg.get());
        }
        call->type = Type::ERROR;
        return Type::ERROR;
    }

    // Argument count check
    if (call->args.size() != fnSym->paramTypes.size()) {
        addError(call->line, call->column,
                 "function '" + call->callee + "' expects " + std::to_string(fnSym->paramTypes.size()) +
                 " arguments, found " + std::to_string(call->args.size()));
    }

    // Argument type check
    size_t limit = std::min(call->args.size(), fnSym->paramTypes.size());
    for (size_t i = 0; i < limit; ++i) {
        Type argType = analyzeExpression(call->args[i].get());
        Type paramType = fnSym->paramTypes[i];

        if (paramType == Type::NUM && argType == Type::DECIMAL) {
            addError(call->args[i]->line, call->args[i]->column,
                     "cannot convert decimal to num without loss of data");
        } else if (!isAssignable(paramType, argType)) {
            addError(call->args[i]->line, call->args[i]->column,
                     "argument " + std::to_string(i + 1) + " of function '" + call->callee +
                     "' expects " + typeToString(paramType) + ", found " + typeToString(argType));
        }
    }

    for (size_t i = limit; i < call->args.size(); ++i) {
        analyzeExpression(call->args[i].get());
    }

    // Rule 25: Call to nothing-function inside an expression
    if (fnSym->returnType == Type::NOTHING && !isStatementContext) {
        addError(call->line, call->column, "function '" + call->callee + "' returns nothing");
        call->type = Type::ERROR;
        return Type::ERROR;
    }

    call->type = fnSym->returnType;
    return call->type;
}

Type SemanticAnalyzer::analyzeIn(InExpr* in) {
    // Rule 23: Optional prompt must be text
    if (in->prompt) {
        Type pt = analyzeExpression(in->prompt.get());
        if (pt != Type::TEXT && pt != Type::ERROR) {
            addError(in->prompt->line, in->prompt->column, "prompt for 'in' must be text");
        }
    }
    in->type = Type::TEXT;
    return Type::TEXT;
}

// ============================================================================
// Rule 27: Path Return Analysis
// ============================================================================
bool SemanticAnalyzer::alwaysReturns(Statement* stmt) {
    if (!stmt) return false;

    switch (stmt->kind) {
        case NodeKind::END_STMT:
            return true;
        case NodeKind::BLOCK: {
            auto* b = static_cast<BlockStmt*>(stmt);
            for (const auto& s : b->statements) {
                if (alwaysReturns(s.get())) return true;
            }
            return false;
        }
        case NodeKind::IF_STMT: {
            auto* ifS = static_cast<IfStmt*>(stmt);
            if (!ifS->elseBranch) return false;
            return alwaysReturns(ifS->thenBranch.get()) && alwaysReturns(ifS->elseBranch.get());
        }
        case NodeKind::WHILE_STMT:
        case NodeKind::REP_STMT:
            return false;
        default:
            return false;
    }
}
