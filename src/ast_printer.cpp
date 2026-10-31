#include "ast_printer.h"

static void printIndent(int indent, std::ostream& out) {
    out << std::string(indent * 2, ' ');
}

void ASTPrinter::print(const ASTNode* node, std::ostream& out, bool showTypes) {
    if (!node) return;
    printNode(node, 0, out, showTypes);
}

void ASTPrinter::printNode(const ASTNode* node, int indent, std::ostream& out, bool showTypes) {
    if (!node) return;

    if (node->kind == NodeKind::PROGRAM) {
        const auto* prog = static_cast<const ProgramNode*>(node);
        printIndent(indent, out);
        out << "Program\n";
        for (const auto& decl : prog->declarations) {
            printStatement(decl.get(), indent + 1, out, showTypes);
        }
        return;
    }

    if (node->kind == NodeKind::PARAM) {
        const auto* param = static_cast<const ParamNode*>(node);
        printIndent(indent, out);
        out << "Param " << param->typeName << " " << param->name;
        if (showTypes && param->resolvedType != Type::ERROR) {
            out << " : " << typeToString(param->resolvedType);
        }
        out << "\n";
        return;
    }

    // Check if expression or statement
    switch (node->kind) {
        case NodeKind::BINARY_EXPR:
        case NodeKind::UNARY_EXPR:
        case NodeKind::LITERAL_EXPR:
        case NodeKind::VARIABLE_EXPR:
        case NodeKind::CALL_EXPR:
        case NodeKind::IN_EXPR:
            printExpression(static_cast<const Expression*>(node), indent, out, showTypes);
            break;
        default:
            printStatement(static_cast<const Statement*>(node), indent, out, showTypes);
            break;
    }
}

void ASTPrinter::printExpression(const Expression* expr, int indent, std::ostream& out, bool showTypes) {
    if (!expr) return;

    std::string typeSuffix;
    if (showTypes && expr->type != Type::ERROR) {
        typeSuffix = " : " + typeToString(expr->type);
    }

    switch (expr->kind) {
        case NodeKind::BINARY_EXPR: {
            const auto* bin = static_cast<const BinaryExpr*>(expr);
            printIndent(indent, out);
            out << "Binary " << bin->op << typeSuffix << "\n";
            printExpression(bin->left.get(), indent + 1, out, showTypes);
            printExpression(bin->right.get(), indent + 1, out, showTypes);
            break;
        }
        case NodeKind::UNARY_EXPR: {
            const auto* un = static_cast<const UnaryExpr*>(expr);
            printIndent(indent, out);
            out << "Unary " << un->op << typeSuffix << "\n";
            printExpression(un->operand.get(), indent + 1, out, showTypes);
            break;
        }
        case NodeKind::LITERAL_EXPR: {
            const auto* lit = static_cast<const LiteralExpr*>(expr);
            printIndent(indent, out);
            out << "Literal " << lit->literalType << " " << lit->value << typeSuffix << "\n";
            break;
        }
        case NodeKind::VARIABLE_EXPR: {
            const auto* var = static_cast<const VariableExpr*>(expr);
            printIndent(indent, out);
            out << "Variable " << var->name;
            if (showTypes && !var->uniqueName.empty() && var->uniqueName != var->name) {
                out << " [" << var->uniqueName << "]";
            }
            out << typeSuffix << "\n";
            break;
        }
        case NodeKind::CALL_EXPR: {
            const auto* call = static_cast<const CallExpr*>(expr);
            printIndent(indent, out);
            out << "Call " << call->callee << typeSuffix << "\n";
            for (const auto& arg : call->args) {
                printExpression(arg.get(), indent + 1, out, showTypes);
            }
            break;
        }
        case NodeKind::IN_EXPR: {
            const auto* inE = static_cast<const InExpr*>(expr);
            printIndent(indent, out);
            out << "In" << typeSuffix << "\n";
            if (inE->prompt) {
                printExpression(inE->prompt.get(), indent + 1, out, showTypes);
            }
            break;
        }
        default:
            printIndent(indent, out);
            out << "UnknownExpr\n";
            break;
    }
}

void ASTPrinter::printStatement(const Statement* stmt, int indent, std::ostream& out, bool showTypes) {
    if (!stmt) return;

    switch (stmt->kind) {
        case NodeKind::FUNC_DECL: {
            const auto* fn = static_cast<const FuncDecl*>(stmt);
            printIndent(indent, out);
            out << "FuncDecl " << fn->returnType << " " << fn->name << "\n";
            for (const auto& p : fn->params) {
                printNode(p.get(), indent + 1, out, showTypes);
            }
            if (fn->body) {
                printStatement(fn->body.get(), indent + 1, out, showTypes);
            }
            break;
        }
        case NodeKind::VAR_DECL: {
            const auto* vd = static_cast<const VarDecl*>(stmt);
            printIndent(indent, out);
            out << "VarDecl ";
            if (vd->isFixed) out << "fixed ";
            out << vd->typeName << " " << vd->name;
            if (showTypes && vd->resolvedType != Type::ERROR) {
                out << " : " << typeToString(vd->resolvedType);
            }
            out << "\n";
            if (vd->initializer) {
                printExpression(vd->initializer.get(), indent + 1, out, showTypes);
            }
            break;
        }
        case NodeKind::ASSIGN_STMT: {
            const auto* asg = static_cast<const AssignStmt*>(stmt);
            printIndent(indent, out);
            out << "Assign " << asg->target << " " << asg->op << "\n";
            if (asg->value) {
                printExpression(asg->value.get(), indent + 1, out, showTypes);
            }
            break;
        }
        case NodeKind::INC_DEC_STMT: {
            const auto* id = static_cast<const IncDecStmt*>(stmt);
            printIndent(indent, out);
            out << "IncDec " << (id->isPrefix ? "prefix " : "postfix ") << id->op << " " << id->name << "\n";
            break;
        }
        case NodeKind::OUT_STMT: {
            const auto* outS = static_cast<const OutStmt*>(stmt);
            printIndent(indent, out);
            out << "Out\n";
            for (const auto& arg : outS->args) {
                printExpression(arg.get(), indent + 1, out, showTypes);
            }
            break;
        }
        case NodeKind::IF_STMT: {
            const auto* ifS = static_cast<const IfStmt*>(stmt);
            printIndent(indent, out);
            out << "If\n";
            printExpression(ifS->condition.get(), indent + 1, out, showTypes);
            if (ifS->thenBranch) {
                printStatement(ifS->thenBranch.get(), indent + 1, out, showTypes);
            }
            if (ifS->elseBranch) {
                printStatement(ifS->elseBranch.get(), indent + 1, out, showTypes);
            }
            break;
        }
        case NodeKind::WHILE_STMT: {
            const auto* ws = static_cast<const WhileStmt*>(stmt);
            printIndent(indent, out);
            out << "While\n";
            printExpression(ws->condition.get(), indent + 1, out, showTypes);
            if (ws->body) {
                printStatement(ws->body.get(), indent + 1, out, showTypes);
            }
            break;
        }
        case NodeKind::REP_STMT: {
            const auto* rs = static_cast<const RepStmt*>(stmt);
            printIndent(indent, out);
            out << "Rep\n";
            if (rs->init) {
                printStatement(rs->init.get(), indent + 1, out, showTypes);
            }
            if (rs->condition) {
                printExpression(rs->condition.get(), indent + 1, out, showTypes);
            }
            if (rs->update) {
                printStatement(rs->update.get(), indent + 1, out, showTypes);
            }
            if (rs->body) {
                printStatement(rs->body.get(), indent + 1, out, showTypes);
            }
            break;
        }
        case NodeKind::STOP_STMT: {
            printIndent(indent, out);
            out << "Stop\n";
            break;
        }
        case NodeKind::SKIP_STMT: {
            printIndent(indent, out);
            out << "Skip\n";
            break;
        }
        case NodeKind::END_STMT: {
            const auto* es = static_cast<const EndStmt*>(stmt);
            printIndent(indent, out);
            out << "End\n";
            if (es->value) {
                printExpression(es->value.get(), indent + 1, out, showTypes);
            }
            break;
        }
        case NodeKind::CALL_STMT: {
            const auto* cs = static_cast<const CallStmt*>(stmt);
            printIndent(indent, out);
            out << "CallStmt\n";
            if (cs->call) {
                printExpression(cs->call.get(), indent + 1, out, showTypes);
            }
            break;
        }
        case NodeKind::BLOCK: {
            const auto* bs = static_cast<const BlockStmt*>(stmt);
            printIndent(indent, out);
            out << "Block\n";
            for (const auto& s : bs->statements) {
                printStatement(s.get(), indent + 1, out, showTypes);
            }
            break;
        }
        default:
            printIndent(indent, out);
            out << "UnknownStmt\n";
            break;
    }
}
