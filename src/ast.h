#ifndef YALL_AST_H
#define YALL_AST_H

#include <string>
#include <vector>
#include <memory>
#include "types.h"

/**
 * @file ast.h
 * @brief Abstract Syntax Tree (AST) node definitions for the Yall language.
 *
 * Design Choice Explanation (Viva Note):
 * --------------------------------------
 * We use an object-oriented node hierarchy with a base class `ASTNode`, distinct
 * sub-hierarchies for `Statement` and `Expression`, and an explicit `NodeKind` enum.
 * 
 * Why this approach over std::variant:
 * 1. Heterogeneous tree structures: An AST is inherently recursive (e.g. BinaryExpr
 *    contains Expressions, Block contains Statements). std::unique_ptr<Base> handles
 *    recursive tree ownership cleanly without cumbersome recursive variant wrappers.
 * 2. Performance & Fast Dispatch: The `NodeKind` tag allows O(1) kind inspection
 *    without runtime RTTI / dynamic_cast overhead.
 * 3. Viva Extensibility: Clear separation between Statement, Expression, and Declaration
 *    makes Phase 3 (Semantic Analysis) and Phase 4 (Code Generation) modular and intuitive.
 */

enum class NodeKind {
    // Declarations & Statements
    PROGRAM,
    FUNC_DECL,
    PARAM,
    VAR_DECL,
    ASSIGN_STMT,
    INC_DEC_STMT,
    OUT_STMT,
    IF_STMT,
    WHILE_STMT,
    REP_STMT,
    STOP_STMT,
    SKIP_STMT,
    END_STMT,
    CALL_STMT,
    BLOCK,

    // Expressions
    BINARY_EXPR,
    UNARY_EXPR,
    LITERAL_EXPR,
    VARIABLE_EXPR,
    CALL_EXPR,
    IN_EXPR
};

// ============================================================================
// Base AST Node
// ============================================================================
struct ASTNode {
    NodeKind kind;
    int line;
    int column;

    ASTNode(NodeKind k, int l, int c) : kind(k), line(l), column(c) {}
    virtual ~ASTNode() = default;
};

// ============================================================================
// Expressions
// ============================================================================
struct Expression : public ASTNode {
    Type type = Type::ERROR; // Annotated by Semantic Analyzer (Phase 3)

    using ASTNode::ASTNode;
};

struct BinaryExpr : public Expression {
    std::string op;
    std::unique_ptr<Expression> left;
    std::unique_ptr<Expression> right;

    BinaryExpr(std::string op, std::unique_ptr<Expression> l, std::unique_ptr<Expression> r, int line, int col)
        : Expression(NodeKind::BINARY_EXPR, line, col), op(std::move(op)), left(std::move(l)), right(std::move(r)) {}
};

struct UnaryExpr : public Expression {
    std::string op;
    std::unique_ptr<Expression> operand;

    UnaryExpr(std::string op, std::unique_ptr<Expression> operand, int line, int col)
        : Expression(NodeKind::UNARY_EXPR, line, col), op(std::move(op)), operand(std::move(operand)) {}
};

struct LiteralExpr : public Expression {
    std::string literalType; // "num", "decimal", "text", "letter", "flag"
    std::string value;

    LiteralExpr(std::string type, std::string val, int line, int col)
        : Expression(NodeKind::LITERAL_EXPR, line, col), literalType(std::move(type)), value(std::move(val)) {}
};

struct VariableExpr : public Expression {
    std::string name;
    std::string uniqueName; // Annotated by Semantic Analyzer (Phase 3)

    VariableExpr(std::string name, int line, int col)
        : Expression(NodeKind::VARIABLE_EXPR, line, col), name(std::move(name)) {}
};

struct CallExpr : public Expression {
    std::string callee;
    std::vector<std::unique_ptr<Expression>> args;

    CallExpr(std::string callee, std::vector<std::unique_ptr<Expression>> args, int line, int col)
        : Expression(NodeKind::CALL_EXPR, line, col), callee(std::move(callee)), args(std::move(args)) {}
};

struct InExpr : public Expression {
    std::unique_ptr<Expression> prompt; // Optional prompt expression, e.g. in("Enter: ")

    InExpr(std::unique_ptr<Expression> prompt, int line, int col)
        : Expression(NodeKind::IN_EXPR, line, col), prompt(std::move(prompt)) {}
};

// ============================================================================
// Statements & Declarations
// ============================================================================
struct Statement : public ASTNode {
    using ASTNode::ASTNode;
};

struct BlockStmt : public Statement {
    std::vector<std::unique_ptr<Statement>> statements;

    BlockStmt(std::vector<std::unique_ptr<Statement>> stmts, int line, int col)
        : Statement(NodeKind::BLOCK, line, col), statements(std::move(stmts)) {}
};

struct VarDecl : public Statement {
    bool isFixed;
    std::string typeName; // "let", "num", "decimal", "letter", "text", "flag"
    std::string name;
    std::string uniqueName;          // Program-unique name (Phase 3)
    Type resolvedType = Type::ERROR; // Resolved type of the variable (Phase 3)
    std::unique_ptr<Expression> initializer; // nullptr if uninitialized

    VarDecl(bool fixed, std::string type, std::string name, std::unique_ptr<Expression> init, int line, int col)
        : Statement(NodeKind::VAR_DECL, line, col), isFixed(fixed), typeName(std::move(type)),
          name(std::move(name)), initializer(std::move(init)) {}
};

struct AssignStmt : public Statement {
    std::string target;
    std::string op; // "=", "+=", "-=", "*=", "/="
    std::string targetUniqueName; // Program-unique name (Phase 3)
    std::unique_ptr<Expression> value;

    AssignStmt(std::string target, std::string op, std::unique_ptr<Expression> val, int line, int col)
        : Statement(NodeKind::ASSIGN_STMT, line, col), target(std::move(target)), op(std::move(op)), value(std::move(val)) {}
};

struct IncDecStmt : public Statement {
    std::string op; // "++" or "--"
    std::string name;
    std::string targetUniqueName; // Program-unique name (Phase 3)
    bool isPrefix;  // true if ++x, false if x++

    IncDecStmt(std::string op, std::string name, bool prefix, int line, int col)
        : Statement(NodeKind::INC_DEC_STMT, line, col), op(std::move(op)), name(std::move(name)), isPrefix(prefix) {}
};

struct OutStmt : public Statement {
    std::vector<std::unique_ptr<Expression>> args;

    OutStmt(std::vector<std::unique_ptr<Expression>> args, int line, int col)
        : Statement(NodeKind::OUT_STMT, line, col), args(std::move(args)) {}
};

struct IfStmt : public Statement {
    std::unique_ptr<Expression> condition;
    std::unique_ptr<BlockStmt> thenBranch;
    std::unique_ptr<Statement> elseBranch; // Either another IfStmt ("other if") or BlockStmt ("other"), or nullptr

    IfStmt(std::unique_ptr<Expression> cond, std::unique_ptr<BlockStmt> thenB,
           std::unique_ptr<Statement> elseB, int line, int col)
        : Statement(NodeKind::IF_STMT, line, col), condition(std::move(cond)),
          thenBranch(std::move(thenB)), elseBranch(std::move(elseB)) {}
};

struct WhileStmt : public Statement {
    std::unique_ptr<Expression> condition;
    std::unique_ptr<BlockStmt> body;

    WhileStmt(std::unique_ptr<Expression> cond, std::unique_ptr<BlockStmt> body, int line, int col)
        : Statement(NodeKind::WHILE_STMT, line, col), condition(std::move(cond)), body(std::move(body)) {}
};

struct RepStmt : public Statement {
    std::unique_ptr<Statement> init;     // VarDecl or AssignStmt
    std::unique_ptr<Expression> condition;
    std::unique_ptr<Statement> update;   // IncDecStmt or AssignStmt
    std::unique_ptr<BlockStmt> body;

    RepStmt(std::unique_ptr<Statement> init, std::unique_ptr<Expression> cond,
            std::unique_ptr<Statement> update, std::unique_ptr<BlockStmt> body, int line, int col)
        : Statement(NodeKind::REP_STMT, line, col), init(std::move(init)),
          condition(std::move(cond)), update(std::move(update)), body(std::move(body)) {}
};

struct StopStmt : public Statement {
    StopStmt(int line, int col) : Statement(NodeKind::STOP_STMT, line, col) {}
};

struct SkipStmt : public Statement {
    SkipStmt(int line, int col) : Statement(NodeKind::SKIP_STMT, line, col) {}
};

struct EndStmt : public Statement {
    std::unique_ptr<Expression> value; // nullptr if bare "end;"

    EndStmt(std::unique_ptr<Expression> val, int line, int col)
        : Statement(NodeKind::END_STMT, line, col), value(std::move(val)) {}
};

struct CallStmt : public Statement {
    std::unique_ptr<CallExpr> call;

    CallStmt(std::unique_ptr<CallExpr> call, int line, int col)
        : Statement(NodeKind::CALL_STMT, line, col), call(std::move(call)) {}
};

struct ParamNode : public ASTNode {
    std::string typeName;
    std::string name;
    std::string uniqueName;          // Program-unique name (Phase 3)
    Type resolvedType = Type::ERROR; // Resolved type (Phase 3)

    ParamNode(std::string type, std::string name, int line, int col)
        : ASTNode(NodeKind::PARAM, line, col), typeName(std::move(type)), name(std::move(name)) {}
};

struct FuncDecl : public Statement {
    std::string returnType; // "num", "decimal", "letter", "text", "flag", "nothing"
    std::string name;       // function name or "start"
    Type resolvedReturnType = Type::ERROR; // Resolved return type (Phase 3)
    std::vector<std::unique_ptr<ParamNode>> params;
    std::unique_ptr<BlockStmt> body;

    FuncDecl(std::string retType, std::string name, std::vector<std::unique_ptr<ParamNode>> params,
             std::unique_ptr<BlockStmt> body, int line, int col)
        : Statement(NodeKind::FUNC_DECL, line, col), returnType(std::move(retType)),
          name(std::move(name)), params(std::move(params)), body(std::move(body)) {}
};

struct ProgramNode : public ASTNode {
    std::vector<std::unique_ptr<Statement>> declarations;

    ProgramNode(std::vector<std::unique_ptr<Statement>> decls, int line, int col)
        : ASTNode(NodeKind::PROGRAM, line, col), declarations(std::move(decls)) {}
};

#endif // YALL_AST_H
