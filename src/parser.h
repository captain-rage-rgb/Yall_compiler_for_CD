#ifndef YALL_PARSER_H
#define YALL_PARSER_H

#include <vector>
#include <string>
#include <memory>
#include "token.h"
#include "ast.h"

/**
 * @struct ParseError
 * @brief Records a syntax error with source location and descriptive message.
 */
struct ParseError {
    int line;
    int column;
    std::string message;
};

/**
 * @class Parser
 * @brief Hand-written Recursive Descent Parser for the Yall language.
 *
 * Implements LL(1) / LL(k) recursive descent parsing to construct the AST.
 * Features:
 *  - Strict grammar adherence (precedence climbing from logicOr to primary)
 *  - Panic-mode error recovery with statement synchronization
 *  - Helpful, context-aware error diagnostics (including found token)
 *  - Early detection of unsupported features (e.g., array subscripting)
 */
class Parser {
public:
    /**
     * @brief Constructs the parser with a token stream.
     * @param tokens Scanned tokens from Phase 1 (Lexer).
     */
    explicit Parser(std::vector<Token> tokens);

    /**
     * @brief Parses the token stream into a complete Program AST node.
     * @return std::unique_ptr<ProgramNode> Root node of the AST (nullptr if critical failure).
     */
    std::unique_ptr<ProgramNode> parse();

    /**
     * @brief Retrieves all syntax errors encountered during parsing.
     * @return const std::vector<ParseError>& List of recorded parse errors.
     */
    const std::vector<ParseError>& errors() const;

private:
    std::vector<Token> tokens_;
    size_t current_ = 0;
    std::vector<ParseError> errors_;

    // Core parser state helpers (as specified)
    bool isAtEnd() const;
    Token peek() const;
    Token peekNext() const;
    Token previous() const;
    Token advance();
    bool check(TokenType type) const;
    bool match(TokenType type);
    Token consume(TokenType type, const std::string& message);

    // Error handling & panic-mode synchronization
    void addError(const Token& tok, const std::string& message);
    void synchronize();
    std::string tokenDescription(const Token& tok) const;

    // Type classification helpers
    bool isReturnType(TokenType type) const;
    bool isVarType(TokenType type) const;
    bool isAssignOp(TokenType type) const;

    // Grammar: Declarations & Statements
    std::unique_ptr<Statement> parseDeclaration();
    std::unique_ptr<FuncDecl> parseFuncDecl();
    std::unique_ptr<Statement> parseStatement();
    std::unique_ptr<VarDecl> parseVarDecl();
    std::unique_ptr<Statement> parseIdentifierStatement();
    std::unique_ptr<IncDecStmt> parsePrefixIncDec();
    std::unique_ptr<OutStmt> parseOutStmt();
    std::unique_ptr<IfStmt> parseIfStmt();
    std::unique_ptr<WhileStmt> parseWhileStmt();
    std::unique_ptr<RepStmt> parseRepStmt();
    std::unique_ptr<StopStmt> parseStopStmt();
    std::unique_ptr<SkipStmt> parseSkipStmt();
    std::unique_ptr<EndStmt> parseEndStmt();
    std::unique_ptr<BlockStmt> parseBlock();

    // Grammar: Expressions (by operator precedence)
    std::unique_ptr<Expression> parseExpression();
    std::unique_ptr<Expression> parseLogicOr();
    std::unique_ptr<Expression> parseLogicAnd();
    std::unique_ptr<Expression> parseEquality();
    std::unique_ptr<Expression> parseComparison();
    std::unique_ptr<Expression> parseTerm();
    std::unique_ptr<Expression> parseFactor();
    std::unique_ptr<Expression> parseUnary();
    std::unique_ptr<Expression> parseCall();
    std::unique_ptr<Expression> parsePrimary();
};

#endif // YALL_PARSER_H
