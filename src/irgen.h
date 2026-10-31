#ifndef YALL_IRGEN_H
#define YALL_IRGEN_H

#include <string>
#include <vector>
#include <unordered_map>
#include "ast.h"
#include "tac.h"

/**
 * @file irgen.h
 * @brief Three-Address Code (TAC) Intermediate Representation Generator for Yall.
 *
 * Implements Phase 4: AST -> TAC translation according to strict language semantics.
 */

class IRGen {
public:
    IRGen() = default;

    /**
     * @brief Generates Three-Address Code for the entire program.
     */
    TACProgram generate(ProgramNode* program);

private:
    struct LoopContext {
        std::string continueLabel;
        std::string breakLabel;
    };

    struct FunctionSig {
        std::string name;
        Type returnType;
        std::vector<Type> paramTypes;
    };

    int tempCounter_ = 0;   // Restarts per function: t1, t2, ...
    int labelCounter_ = 0;  // Unique across entire program: L1, L2, ...

    std::unordered_map<std::string, FunctionSig> funcSigs_;
    std::unordered_map<std::string, Type> varTypes_; // Map from uniqueName to Type
    std::vector<LoopContext> loopStack_;

    TACFunction* currentFunc_ = nullptr;

    // Helper functions
    std::string newTemp();
    std::string newLabel();
    Operand widenToDecimal(const Operand& op, int line);

    void emit(Opcode op, Operand dest, Operand arg1, Operand arg2, Type type, int line);
    void emit(Opcode op, Operand arg1, Type type, int line);
    void emit(Opcode op, Operand arg1, int line);
    void emit(Opcode op, int line);

    // AST visitors
    void generateFuncDecl(FuncDecl* fn);
    void generateStatement(Statement* stmt);
    void generateBlock(BlockStmt* block);
    void generateVarDecl(VarDecl* vd);
    void generateAssign(AssignStmt* asg);
    void generateIncDec(IncDecStmt* id);
    void generateOut(OutStmt* out);
    void generateIf(IfStmt* ifS);
    void generateWhile(WhileStmt* ws);
    void generateRep(RepStmt* rs);
    void generateEnd(EndStmt* es);
    void generateCallStmt(CallStmt* cs);

    Operand generateExpression(Expression* expr);
    Operand generateBinary(BinaryExpr* bin);
    Operand generateUnary(UnaryExpr* un);
    Operand generateLiteral(LiteralExpr* lit);
    Operand generateVariable(VariableExpr* var);
    Operand generateCallExpr(CallExpr* call);
    Operand generateIn(InExpr* in);
};

#endif // YALL_IRGEN_H
