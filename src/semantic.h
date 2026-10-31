#ifndef YALL_SEMANTIC_H
#define YALL_SEMANTIC_H

#include <vector>
#include <string>
#include "ast.h"
#include "symbol_table.h"

/**
 * @struct SemanticError
 * @brief Represents a compile-time semantic violation.
 */
struct SemanticError {
    int line;
    int column;
    std::string message;
};

/**
 * @struct SemanticWarning
 * @brief Represents a non-fatal compile-time diagnostic.
 */
struct SemanticWarning {
    int line;
    int column;
    std::string message;
};

/**
 * @class SemanticAnalyzer
 * @brief AST Visitor / Walker performing comprehensive semantic validation.
 *
 * Implements two passes:
 *  - Pass 1: Collects top-level function signatures for forward references & recursion.
 *  - Pass 2: Type checks declarations, expressions, control flow, scoping, and returns.
 */
class SemanticAnalyzer {
public:
    SemanticAnalyzer();

    /**
     * @brief Performs complete semantic analysis on the program AST.
     * @param program Root ProgramNode.
     */
    void analyze(ProgramNode* program);

    const std::vector<SemanticError>& errors() const;
    const std::vector<SemanticWarning>& warnings() const;
    const SymbolTable& symbolTable() const;

private:
    SymbolTable symTab_;
    std::vector<SemanticError> errors_;
    std::vector<SemanticWarning> warnings_;

    Type currentFunctionReturnType_ = Type::NOTHING;
    std::string currentFunctionName_;
    int loopDepth_ = 0;

    void addError(int line, int col, const std::string& message);
    void addWarning(int line, int col, const std::string& message);

    // Pass 1: Global function declarations
    void pass1CollectFunctions(ProgramNode* program);

    // Pass 2: Bodies, statements, and expressions
    void pass2AnalyzeProgram(ProgramNode* program);
    void analyzeFunction(FuncDecl* fn);
    void analyzeStatement(Statement* stmt);
    void analyzeBlock(BlockStmt* block, bool createScope = true, const std::string& scopeName = "Block");
    Type analyzeExpression(Expression* expr);

    // Statement visitors
    void analyzeVarDecl(VarDecl* vd);
    void analyzeAssign(AssignStmt* asg);
    void analyzeIncDec(IncDecStmt* id);
    void analyzeOut(OutStmt* out);
    void analyzeIf(IfStmt* ifS);
    void analyzeWhile(WhileStmt* ws);
    void analyzeRep(RepStmt* rs);
    void analyzeEnd(EndStmt* es);
    void analyzeCallStmt(CallStmt* cs);

    // Expression visitors
    Type analyzeBinary(BinaryExpr* bin);
    Type analyzeUnary(UnaryExpr* un);
    Type analyzeLiteral(LiteralExpr* lit);
    Type analyzeVariable(VariableExpr* var);
    Type analyzeCall(CallExpr* call, bool isStatementContext = false);
    Type analyzeIn(InExpr* in);

    // Semantic flow checks
    bool alwaysReturns(Statement* stmt);
};

#endif // YALL_SEMANTIC_H
