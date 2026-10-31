#include "parser.h"
#include <stdexcept>

/**
 * @brief Internal exception used solely for panic-mode unwinding to synchronization points.
 */
struct ParseException : public std::runtime_error {
    ParseException() : std::runtime_error("Parse error") {}
};

// ============================================================================
// Constructor
// ============================================================================
Parser::Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

// ============================================================================
// Core Navigation & Inspection Helpers
// ============================================================================
bool Parser::isAtEnd() const {
    return current_ >= tokens_.size() || tokens_[current_].type == TokenType::TOKEN_EOF;
}

Token Parser::peek() const {
    if (current_ >= tokens_.size()) {
        return Token{TokenType::TOKEN_EOF, "", 0, 0};
    }
    return tokens_[current_];
}

Token Parser::peekNext() const {
    if (current_ + 1 >= tokens_.size()) {
        return Token{TokenType::TOKEN_EOF, "", 0, 0};
    }
    return tokens_[current_ + 1];
}

Token Parser::previous() const {
    if (current_ == 0) {
        return Token{TokenType::TOKEN_EOF, "", 0, 0};
    }
    return tokens_[current_ - 1];
}

Token Parser::advance() {
    if (!isAtEnd()) {
        current_++;
    }
    return previous();
}

bool Parser::check(TokenType type) const {
    if (isAtEnd() && type != TokenType::TOKEN_EOF) return false;
    return peek().type == type;
}

bool Parser::match(TokenType type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

Token Parser::consume(TokenType type, const std::string& message) {
    if (check(type)) {
        return advance();
    }
    std::string fullMsg = message + ", found '" + tokenDescription(peek()) + "'";
    addError(peek(), fullMsg);
    throw ParseException();
}

void Parser::addError(const Token& tok, const std::string& message) {
    // Avoid emitting redundant duplicate errors at the exact same location
    if (!errors_.empty() && errors_.back().line == tok.line &&
        errors_.back().column == tok.column && errors_.back().message == message) {
        return;
    }
    errors_.push_back(ParseError{tok.line, tok.column, message});
}

std::string Parser::tokenDescription(const Token& tok) const {
    if (tok.type == TokenType::TOKEN_EOF) return "EOF";
    if (!tok.lexeme.empty()) return tok.lexeme;
    return tokenTypeToString(tok.type);
}

const std::vector<ParseError>& Parser::errors() const {
    return errors_;
}

// ============================================================================
// Type Classification Helpers
// ============================================================================
bool Parser::isReturnType(TokenType type) const {
    return type == TokenType::KW_NUM ||
           type == TokenType::KW_DECIMAL ||
           type == TokenType::KW_LETTER ||
           type == TokenType::KW_TEXT ||
           type == TokenType::KW_FLAG ||
           type == TokenType::KW_NOTHING;
}

bool Parser::isVarType(TokenType type) const {
    return type == TokenType::KW_NUM ||
           type == TokenType::KW_DECIMAL ||
           type == TokenType::KW_LETTER ||
           type == TokenType::KW_TEXT ||
           type == TokenType::KW_FLAG;
}

bool Parser::isAssignOp(TokenType type) const {
    return type == TokenType::ASSIGN ||
           type == TokenType::PLUS_ASSIGN ||
           type == TokenType::MINUS_ASSIGN ||
           type == TokenType::STAR_ASSIGN ||
           type == TokenType::SLASH_ASSIGN;
}

// ============================================================================
// Panic-Mode Recovery / Synchronization
// ============================================================================
void Parser::synchronize() {
    if (isAtEnd()) return;

    // Advance at least one token past the fault point
    advance();

    while (!isAtEnd()) {
        // If the token just consumed was a semicolon, statement has ended
        if (previous().type == TokenType::SEMICOLON) {
            return;
        }

        // If the next token begins a new declaration/statement or closes a block
        switch (peek().type) {
            case TokenType::KW_FUNC:
            case TokenType::KW_LET:
            case TokenType::KW_NUM:
            case TokenType::KW_DECIMAL:
            case TokenType::KW_LETTER:
            case TokenType::KW_TEXT:
            case TokenType::KW_FLAG:
            case TokenType::KW_FIXED:
            case TokenType::KW_IF:
            case TokenType::KW_WHILE:
            case TokenType::KW_REP:
            case TokenType::KW_OUT:
            case TokenType::KW_END:
            case TokenType::KW_STOP:
            case TokenType::KW_SKIP:
            case TokenType::RIGHT_BRACE:
                return;
            default:
                break;
        }

        advance();
    }
}

// ============================================================================
// Main Parsing Entry Point
// ============================================================================
std::unique_ptr<ProgramNode> Parser::parse() {
    std::vector<std::unique_ptr<Statement>> decls;

    while (!isAtEnd()) {
        try {
            auto decl = parseDeclaration();
            if (decl) {
                decls.push_back(std::move(decl));
            }
        } catch (const ParseException&) {
            synchronize();
        }
    }

    int line = decls.empty() ? 1 : decls.front()->line;
    int col = decls.empty() ? 1 : decls.front()->column;
    return std::make_unique<ProgramNode>(std::move(decls), line, col);
}

// ============================================================================
// Grammar: Declarations & Statements
// ============================================================================
std::unique_ptr<Statement> Parser::parseDeclaration() {
    if (check(TokenType::KW_FUNC)) {
        return parseFuncDecl();
    }
    return parseStatement();
}

std::unique_ptr<FuncDecl> Parser::parseFuncDecl() {
    Token funcTok = advance(); // consume 'func'

    // Form 2: "func" "start" "(" ")" block
    if (match(TokenType::KW_START)) {
        consume(TokenType::LEFT_PAREN, "expected '(' after 'start'");
        consume(TokenType::RIGHT_PAREN, "expected ')' after '(' in start function");
        auto body = parseBlock();
        return std::make_unique<FuncDecl>("nothing", "start",
                                          std::vector<std::unique_ptr<ParamNode>>(),
                                          std::move(body), funcTok.line, funcTok.column);
    }

    // Form 1: "func" returnType IDENTIFIER "(" params? ")" block
    if (!isReturnType(peek().type)) {
        addError(peek(), "expected return type for function, found '" + tokenDescription(peek()) + "'");
        throw ParseException();
    }

    Token retTypeTok = advance();
    Token nameTok = consume(TokenType::IDENTIFIER, "expected function name after return type");
    consume(TokenType::LEFT_PAREN, "expected '(' after function name");

    std::vector<std::unique_ptr<ParamNode>> params;
    if (!check(TokenType::RIGHT_PAREN)) {
        // Parse first parameter
        if (!isVarType(peek().type)) {
            addError(peek(), "expected a type name before parameter name, found '" + tokenDescription(peek()) + "'");
            throw ParseException();
        }
        Token pType = advance();
        Token pName = consume(TokenType::IDENTIFIER, "expected parameter name after type");
        params.push_back(std::make_unique<ParamNode>(pType.lexeme, pName.lexeme, pType.line, pType.column));

        // Parse remaining parameters
        while (match(TokenType::COMMA)) {
            if (!isVarType(peek().type)) {
                addError(peek(), "expected a type name before parameter name, found '" + tokenDescription(peek()) + "'");
                throw ParseException();
            }
            Token nextPType = advance();
            Token nextPName = consume(TokenType::IDENTIFIER, "expected parameter name after type");
            params.push_back(std::make_unique<ParamNode>(nextPType.lexeme, nextPName.lexeme, nextPType.line, nextPType.column));
        }
    }

    consume(TokenType::RIGHT_PAREN, "expected ')' after parameters");
    auto body = parseBlock();
    return std::make_unique<FuncDecl>(retTypeTok.lexeme, nameTok.lexeme,
                                      std::move(params), std::move(body), funcTok.line, funcTok.column);
}

std::unique_ptr<Statement> Parser::parseStatement() {
    // Array access statement attempt: [ ... ]
    if (check(TokenType::LEFT_BRACKET) || check(TokenType::RIGHT_BRACKET)) {
        Token b = advance();
        addError(b, "arrays are not supported yet");
        while (!isAtEnd() && !check(TokenType::SEMICOLON) && !check(TokenType::RIGHT_BRACE)) {
            advance();
        }
        if (check(TokenType::SEMICOLON)) advance();
        throw ParseException();
    }

    // varDecl with fixed or explicit type or let
    if (check(TokenType::KW_FIXED) || check(TokenType::KW_LET) || isVarType(peek().type)) {
        return parseVarDecl();
    }

    // ifStmt
    if (check(TokenType::KW_IF)) {
        return parseIfStmt();
    }

    // whileStmt
    if (check(TokenType::KW_WHILE)) {
        return parseWhileStmt();
    }

    // repStmt
    if (check(TokenType::KW_REP)) {
        return parseRepStmt();
    }

    // outStmt
    if (check(TokenType::KW_OUT)) {
        return parseOutStmt();
    }

    // stopStmt
    if (check(TokenType::KW_STOP)) {
        return parseStopStmt();
    }

    // skipStmt
    if (check(TokenType::KW_SKIP)) {
        return parseSkipStmt();
    }

    // endStmt
    if (check(TokenType::KW_END)) {
        return parseEndStmt();
    }

    // block
    if (check(TokenType::LEFT_BRACE)) {
        return parseBlock();
    }

    // prefix ++ or --
    if (check(TokenType::INCREMENT) || check(TokenType::DECREMENT)) {
        return parsePrefixIncDec();
    }

    // Statement beginning with IDENTIFIER: assignStmt, postfix incDecStmt, or callStmt
    if (check(TokenType::IDENTIFIER)) {
        return parseIdentifierStatement();
    }

    // Unexpected statement start
    addError(peek(), "expected statement, found '" + tokenDescription(peek()) + "'");
    throw ParseException();
}

std::unique_ptr<VarDecl> Parser::parseVarDecl() {
    Token startTok = peek();
    bool isFixed = match(TokenType::KW_FIXED);
    bool hasLet = match(TokenType::KW_LET);
    std::string typeName;

    if (isVarType(peek().type)) {
        typeName = advance().lexeme;
        Token id = consume(TokenType::IDENTIFIER, "expected identifier after type '" + typeName + "'");
        std::unique_ptr<Expression> init = nullptr;
        if (match(TokenType::ASSIGN)) {
            init = parseExpression();
        } else if (hasLet) {
            consume(TokenType::ASSIGN, "expected '=' after identifier in 'let' declaration");
        }
        consume(TokenType::SEMICOLON, "expected ';' after variable declaration");
        return std::make_unique<VarDecl>(isFixed, typeName, id.lexeme, std::move(init), startTok.line, startTok.column);
    } else if (hasLet) {
        Token id = consume(TokenType::IDENTIFIER, "expected identifier after 'let'");
        consume(TokenType::ASSIGN, "expected '=' after identifier in 'let' declaration");
        auto init = parseExpression();
        consume(TokenType::SEMICOLON, "expected ';' after variable declaration");
        return std::make_unique<VarDecl>(isFixed, "let", id.lexeme, std::move(init), startTok.line, startTok.column);
    }

    addError(peek(), "expected type or 'let' in variable declaration, found '" + tokenDescription(peek()) + "'");
    throw ParseException();
}

std::unique_ptr<Statement> Parser::parseIdentifierStatement() {
    Token id = advance(); // consume IDENTIFIER

    // 1. Array indexing not supported: a[...]
    if (check(TokenType::LEFT_BRACKET)) {
        Token b = advance();
        addError(b, "arrays are not supported yet");
        while (!isAtEnd() && !check(TokenType::RIGHT_BRACKET) && !check(TokenType::SEMICOLON)) advance();
        if (check(TokenType::RIGHT_BRACKET)) advance();
        if (check(TokenType::SEMICOLON)) advance();
        throw ParseException();
    }

    // 2. assignStmt: IDENTIFIER assignOp expression ";"
    if (isAssignOp(peek().type)) {
        Token op = advance();
        auto val = parseExpression();
        consume(TokenType::SEMICOLON, "expected ';' after assignment");
        return std::make_unique<AssignStmt>(id.lexeme, op.lexeme, std::move(val), id.line, id.column);
    }

    // 3. postfix incDecStmt: IDENTIFIER ( "++" | "--" ) ";"
    if (check(TokenType::INCREMENT) || check(TokenType::DECREMENT)) {
        Token op = advance();
        consume(TokenType::SEMICOLON, "expected ';' after '" + op.lexeme + "'");
        return std::make_unique<IncDecStmt>(op.lexeme, id.lexeme, false, id.line, id.column);
    }

    // 4. callStmt: IDENTIFIER "(" arguments? ")" ";"
    if (match(TokenType::LEFT_PAREN)) {
        std::vector<std::unique_ptr<Expression>> args;
        if (!check(TokenType::RIGHT_PAREN)) {
            args.push_back(parseExpression());
            while (match(TokenType::COMMA)) {
                args.push_back(parseExpression());
            }
        }
        consume(TokenType::RIGHT_PAREN, "expected ')' after arguments");
        consume(TokenType::SEMICOLON, "expected ';' after call");
        auto call = std::make_unique<CallExpr>(id.lexeme, std::move(args), id.line, id.column);
        return std::make_unique<CallStmt>(std::move(call), id.line, id.column);
    }

    addError(peek(), "expected assignment, '++', '--', or '(' after identifier, found '" + tokenDescription(peek()) + "'");
    throw ParseException();
}

std::unique_ptr<IncDecStmt> Parser::parsePrefixIncDec() {
    Token op = advance(); // consume '++' or '--'
    Token id = consume(TokenType::IDENTIFIER, "expected identifier after '" + op.lexeme + "'");
    consume(TokenType::SEMICOLON, "expected ';' after '" + op.lexeme + id.lexeme + "'");
    return std::make_unique<IncDecStmt>(op.lexeme, id.lexeme, true, op.line, op.column);
}

std::unique_ptr<OutStmt> Parser::parseOutStmt() {
    Token outTok = advance(); // consume 'out'
    consume(TokenType::LEFT_PAREN, "expected '(' after 'out'");

    std::vector<std::unique_ptr<Expression>> args;
    if (!check(TokenType::RIGHT_PAREN)) {
        args.push_back(parseExpression());
        while (match(TokenType::COMMA)) {
            args.push_back(parseExpression());
        }
    }

    consume(TokenType::RIGHT_PAREN, "expected ')' after 'out' arguments");
    consume(TokenType::SEMICOLON, "expected ';' after 'out' statement");
    return std::make_unique<OutStmt>(std::move(args), outTok.line, outTok.column);
}

std::unique_ptr<IfStmt> Parser::parseIfStmt() {
    Token ifTok = advance(); // consume 'if'
    consume(TokenType::LEFT_PAREN, "expected '(' after 'if'");
    auto cond = parseExpression();

    // Avoid cascading error if closing ')' was omitted right before '{'
    if (!check(TokenType::RIGHT_PAREN) && check(TokenType::LEFT_BRACE)) {
        addError(peek(), "expected ')' after 'if' condition, found '{'");
    } else {
        consume(TokenType::RIGHT_PAREN, "expected ')' after 'if' condition");
    }

    auto thenBranch = parseBlock();
    std::unique_ptr<Statement> elseBranch = nullptr;

    if (match(TokenType::KW_OTHER)) {
        if (check(TokenType::KW_IF)) {
            elseBranch = parseIfStmt();
        } else if (check(TokenType::LEFT_BRACE)) {
            elseBranch = parseBlock();
        } else {
            addError(peek(), "expected 'if' or '{' after 'other', found '" + tokenDescription(peek()) + "'");
            throw ParseException();
        }
    }

    return std::make_unique<IfStmt>(std::move(cond), std::move(thenBranch),
                                    std::move(elseBranch), ifTok.line, ifTok.column);
}

std::unique_ptr<WhileStmt> Parser::parseWhileStmt() {
    Token whileTok = advance(); // consume 'while'

    // Avoid cascading error if opening '(' was omitted
    if (!check(TokenType::LEFT_PAREN)) {
        addError(peek(), "expected '(' after 'while', found '" + tokenDescription(peek()) + "'");
    } else {
        advance(); // consume '('
    }

    auto cond = parseExpression();
    consume(TokenType::RIGHT_PAREN, "expected ')' after 'while' condition");
    auto body = parseBlock();
    return std::make_unique<WhileStmt>(std::move(cond), std::move(body), whileTok.line, whileTok.column);
}

std::unique_ptr<RepStmt> Parser::parseRepStmt() {
    Token repTok = advance(); // consume 'rep'
    consume(TokenType::LEFT_PAREN, "expected '(' after 'rep'");

    // repInit -> "let" IDENTIFIER "=" expression | varType IDENTIFIER "=" expression | IDENTIFIER "=" expression
    std::unique_ptr<Statement> init = nullptr;
    bool repLet = match(TokenType::KW_LET);
    if (isVarType(peek().type)) {
        Token typeTok = advance();
        Token id = consume(TokenType::IDENTIFIER, "expected identifier in 'rep' initialization");
        consume(TokenType::ASSIGN, "expected '=' in 'rep' initialization");
        auto val = parseExpression();
        init = std::make_unique<VarDecl>(false, typeTok.lexeme, id.lexeme, std::move(val), typeTok.line, typeTok.column);
    } else if (repLet) {
        Token id = consume(TokenType::IDENTIFIER, "expected identifier in 'rep' initialization");
        consume(TokenType::ASSIGN, "expected '=' in 'rep' initialization");
        auto val = parseExpression();
        init = std::make_unique<VarDecl>(false, "let", id.lexeme, std::move(val), id.line, id.column);
    } else if (check(TokenType::IDENTIFIER)) {
        Token id = advance();
        consume(TokenType::ASSIGN, "expected '=' in 'rep' initialization");
        auto val = parseExpression();
        init = std::make_unique<AssignStmt>(id.lexeme, "=", std::move(val), id.line, id.column);
    } else {
        addError(peek(), "expected loop initialization in 'rep', found '" + tokenDescription(peek()) + "'");
        throw ParseException();
    }

    consume(TokenType::SEMICOLON, "expected ';' after 'rep' initialization");
    auto cond = parseExpression();
    consume(TokenType::SEMICOLON, "expected ';' after 'rep' condition");

    // repUpdate -> ( "++" | "--" ) IDENTIFIER | IDENTIFIER ( "++" | "--" ) | IDENTIFIER assignOp expression
    std::unique_ptr<Statement> update = nullptr;
    if (check(TokenType::INCREMENT) || check(TokenType::DECREMENT)) {
        Token op = advance();
        Token id = consume(TokenType::IDENTIFIER, "expected identifier after '" + op.lexeme + "' in 'rep' update");
        update = std::make_unique<IncDecStmt>(op.lexeme, id.lexeme, true, op.line, op.column);
    } else if (check(TokenType::IDENTIFIER)) {
        Token id = advance();
        if (check(TokenType::INCREMENT) || check(TokenType::DECREMENT)) {
            Token op = advance();
            update = std::make_unique<IncDecStmt>(op.lexeme, id.lexeme, false, id.line, id.column);
        } else if (isAssignOp(peek().type)) {
            Token op = advance();
            auto val = parseExpression();
            update = std::make_unique<AssignStmt>(id.lexeme, op.lexeme, std::move(val), id.line, id.column);
        } else {
            addError(peek(), "expected '++', '--' or assignment in 'rep' update, found '" + tokenDescription(peek()) + "'");
            throw ParseException();
        }
    } else {
        addError(peek(), "expected loop update in 'rep', found '" + tokenDescription(peek()) + "'");
        throw ParseException();
    }

    consume(TokenType::RIGHT_PAREN, "expected ')' after 'rep' update");
    auto body = parseBlock();
    return std::make_unique<RepStmt>(std::move(init), std::move(cond), std::move(update),
                                    std::move(body), repTok.line, repTok.column);
}

std::unique_ptr<StopStmt> Parser::parseStopStmt() {
    Token stopTok = advance(); // consume 'stop'
    consume(TokenType::SEMICOLON, "expected ';' after 'stop'");
    return std::make_unique<StopStmt>(stopTok.line, stopTok.column);
}

std::unique_ptr<SkipStmt> Parser::parseSkipStmt() {
    Token skipTok = advance(); // consume 'skip'
    consume(TokenType::SEMICOLON, "expected ';' after 'skip'");
    return std::make_unique<SkipStmt>(skipTok.line, skipTok.column);
}

std::unique_ptr<EndStmt> Parser::parseEndStmt() {
    Token endTok = advance(); // consume 'end'
    std::unique_ptr<Expression> val = nullptr;

    if (!check(TokenType::SEMICOLON)) {
        val = parseExpression();
    }

    consume(TokenType::SEMICOLON, "expected ';' after 'end'");
    return std::make_unique<EndStmt>(std::move(val), endTok.line, endTok.column);
}

std::unique_ptr<BlockStmt> Parser::parseBlock() {
    Token brace = consume(TokenType::LEFT_BRACE, "expected '{' to start block");
    std::vector<std::unique_ptr<Statement>> stmts;

    while (!check(TokenType::RIGHT_BRACE) && !isAtEnd()) {
        try {
            auto stmt = parseStatement();
            if (stmt) {
                stmts.push_back(std::move(stmt));
            }
        } catch (const ParseException&) {
            synchronize();
        }
    }

    consume(TokenType::RIGHT_BRACE, "expected '}' after block");
    return std::make_unique<BlockStmt>(std::move(stmts), brace.line, brace.column);
}

// ============================================================================
// Grammar: Expressions (by Operator Precedence)
// ============================================================================
std::unique_ptr<Expression> Parser::parseExpression() {
    return parseLogicOr();
}

std::unique_ptr<Expression> Parser::parseLogicOr() {
    auto expr = parseLogicAnd();

    while (match(TokenType::OR_OR)) {
        Token op = previous();
        auto right = parseLogicAnd();
        expr = std::make_unique<BinaryExpr>(op.lexeme, std::move(expr), std::move(right), op.line, op.column);
    }

    return expr;
}

std::unique_ptr<Expression> Parser::parseLogicAnd() {
    auto expr = parseEquality();

    while (match(TokenType::AND_AND)) {
        Token op = previous();
        auto right = parseEquality();
        expr = std::make_unique<BinaryExpr>(op.lexeme, std::move(expr), std::move(right), op.line, op.column);
    }

    return expr;
}

std::unique_ptr<Expression> Parser::parseEquality() {
    auto expr = parseComparison();

    while (check(TokenType::EQUAL_EQUAL) || check(TokenType::BANG_EQUAL)) {
        Token op = advance();
        auto right = parseComparison();
        expr = std::make_unique<BinaryExpr>(op.lexeme, std::move(expr), std::move(right), op.line, op.column);
    }

    return expr;
}

std::unique_ptr<Expression> Parser::parseComparison() {
    auto expr = parseTerm();

    while (check(TokenType::LESS) || check(TokenType::GREATER) ||
           check(TokenType::LESS_EQUAL) || check(TokenType::GREATER_EQUAL)) {
        Token op = advance();
        auto right = parseTerm();
        expr = std::make_unique<BinaryExpr>(op.lexeme, std::move(expr), std::move(right), op.line, op.column);
    }

    return expr;
}

std::unique_ptr<Expression> Parser::parseTerm() {
    auto expr = parseFactor();

    while (check(TokenType::PLUS) || check(TokenType::MINUS)) {
        Token op = advance();
        auto right = parseFactor();
        expr = std::make_unique<BinaryExpr>(op.lexeme, std::move(expr), std::move(right), op.line, op.column);
    }

    return expr;
}

std::unique_ptr<Expression> Parser::parseFactor() {
    auto expr = parseUnary();

    while (check(TokenType::STAR) || check(TokenType::SLASH) || check(TokenType::PERCENT)) {
        Token op = advance();
        auto right = parseUnary();
        expr = std::make_unique<BinaryExpr>(op.lexeme, std::move(expr), std::move(right), op.line, op.column);
    }

    return expr;
}

std::unique_ptr<Expression> Parser::parseUnary() {
    if (check(TokenType::MINUS) || check(TokenType::BANG)) {
        Token op = advance();
        auto operand = parseUnary();
        return std::make_unique<UnaryExpr>(op.lexeme, std::move(operand), op.line, op.column);
    }

    return parseCall();
}

std::unique_ptr<Expression> Parser::parseCall() {
    // Function call expression: IDENTIFIER "(" arguments? ")"
    if (check(TokenType::IDENTIFIER) && peekNext().type == TokenType::LEFT_PAREN) {
        Token id = advance();
        advance(); // consume '('
        std::vector<std::unique_ptr<Expression>> args;
        if (!check(TokenType::RIGHT_PAREN)) {
            args.push_back(parseExpression());
            while (match(TokenType::COMMA)) {
                args.push_back(parseExpression());
            }
        }
        consume(TokenType::RIGHT_PAREN, "expected ')' after arguments");
        return std::make_unique<CallExpr>(id.lexeme, std::move(args), id.line, id.column);
    }

    auto expr = parsePrimary();

    // Check for array indexing attempt: expr[0]
    if (check(TokenType::LEFT_BRACKET)) {
        Token b = advance();
        addError(b, "arrays are not supported yet");
        while (!isAtEnd() && !check(TokenType::RIGHT_BRACKET) && !check(TokenType::SEMICOLON)) {
            advance();
        }
        if (check(TokenType::RIGHT_BRACKET)) advance();
        return expr;
    }

    return expr;
}

std::unique_ptr<Expression> Parser::parsePrimary() {
    // Literals
    if (check(TokenType::NUM_LITERAL)) {
        Token t = advance();
        return std::make_unique<LiteralExpr>("num", t.lexeme, t.line, t.column);
    }
    if (check(TokenType::DECIMAL_LITERAL)) {
        Token t = advance();
        return std::make_unique<LiteralExpr>("decimal", t.lexeme, t.line, t.column);
    }
    if (check(TokenType::TEXT_LITERAL)) {
        Token t = advance();
        return std::make_unique<LiteralExpr>("text", t.lexeme, t.line, t.column);
    }
    if (check(TokenType::LETTER_LITERAL)) {
        Token t = advance();
        return std::make_unique<LiteralExpr>("letter", t.lexeme, t.line, t.column);
    }
    if (check(TokenType::KW_YES)) {
        Token t = advance();
        return std::make_unique<LiteralExpr>("flag", "yes", t.line, t.column);
    }
    if (check(TokenType::KW_NO)) {
        Token t = advance();
        return std::make_unique<LiteralExpr>("flag", "no", t.line, t.column);
    }

    // inExpr: "in" "(" expression? ")"
    if (check(TokenType::KW_IN)) {
        Token inTok = advance();
        consume(TokenType::LEFT_PAREN, "expected '(' after 'in'");
        std::unique_ptr<Expression> prompt = nullptr;
        if (!check(TokenType::RIGHT_PAREN)) {
            prompt = parseExpression();
        }
        consume(TokenType::RIGHT_PAREN, "expected ')' after 'in'");
        return std::make_unique<InExpr>(std::move(prompt), inTok.line, inTok.column);
    }

    // Variable reference
    if (check(TokenType::IDENTIFIER)) {
        Token t = advance();
        return std::make_unique<VariableExpr>(t.lexeme, t.line, t.column);
    }

    // Grouped expression: "(" expression ")"
    if (match(TokenType::LEFT_PAREN)) {
        auto expr = parseExpression();
        consume(TokenType::RIGHT_PAREN, "expected ')' after expression");
        return expr;
    }

    // Array brackets: '[' ']'
    if (check(TokenType::LEFT_BRACKET)) {
        Token b = advance();
        addError(b, "arrays are not supported yet");
        while (!isAtEnd() && !check(TokenType::RIGHT_BRACKET) && !check(TokenType::SEMICOLON)) {
            advance();
        }
        if (check(TokenType::RIGHT_BRACKET)) advance();
        throw ParseException();
    }

    // Unexpected token
    addError(peek(), "expected expression, found '" + tokenDescription(peek()) + "'");
    throw ParseException();
}
