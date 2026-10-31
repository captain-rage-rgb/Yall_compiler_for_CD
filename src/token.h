#ifndef YALL_TOKEN_H
#define YALL_TOKEN_H

#include <string>

/**
 * @brief Represents every unique token type recognized in the Yall language.
 *
 * Each keyword, literal, operator, punctuation mark, and EOF has its own
 * distinct enum value.
 */
enum class TokenType {
    // === Keywords: Types ===
    KW_NUM,         // 'num'
    KW_DECIMAL,     // 'decimal'
    KW_LETTER,      // 'letter'
    KW_TEXT,        // 'text'
    KW_FLAG,        // 'flag'
    KW_NOTHING,     // 'nothing'
    KW_FIXED,       // 'fixed'
    KW_LET,         // 'let'

    // === Keywords: Values ===
    KW_YES,         // 'yes'
    KW_NO,          // 'no'

    // === Keywords: I/O ===
    KW_OUT,         // 'out'
    KW_IN,          // 'in'

    // === Keywords: Control Flow ===
    KW_IF,          // 'if'
    KW_OTHER,       // 'other'
    KW_WHILE,       // 'while'
    KW_REP,         // 'rep'
    KW_STOP,        // 'stop'
    KW_SKIP,        // 'skip'
    KW_END,         // 'end'

    // === Keywords: Functions ===
    KW_FUNC,        // 'func'
    KW_START,       // 'start'

    // === Identifiers and Literals ===
    IDENTIFIER,         // e.g. variable or function name
    NUM_LITERAL,        // integer literal, e.g. 42
    DECIMAL_LITERAL,    // floating-point literal, e.g. 3.14
    TEXT_LITERAL,       // double-quoted string literal, e.g. "hello"
    LETTER_LITERAL,     // single-quoted char literal, e.g. 'a'

    // === Operators ===
    PLUS,               // '+'
    MINUS,              // '-'
    STAR,               // '*'
    SLASH,              // '/'
    PERCENT,            // '%'
    ASSIGN,             // '='
    PLUS_ASSIGN,        // '+='
    MINUS_ASSIGN,       // '-='
    STAR_ASSIGN,        // '*='
    SLASH_ASSIGN,       // '/='
    INCREMENT,          // '++'
    DECREMENT,          // '--'
    EQUAL_EQUAL,        // '=='
    BANG_EQUAL,         // '!='
    LESS,               // '<'
    GREATER,            // '>'
    LESS_EQUAL,         // '<='
    GREATER_EQUAL,      // '>='
    AND_AND,            // '&&'
    OR_OR,              // '||'
    BANG,               // '!'

    // === Punctuation ===
    SEMICOLON,          // ';'
    COMMA,              // ','
    LEFT_PAREN,         // '('
    RIGHT_PAREN,        // ')'
    LEFT_BRACE,         // '{'
    RIGHT_BRACE,        // '}'
    LEFT_BRACKET,       // '['
    RIGHT_BRACKET,      // ']'

    // === End of File ===
    TOKEN_EOF           // End of input stream
};

/**
 * @brief Represents a token emitted by the Yall lexer.
 * 
 * Line and column are 1-indexed. For string and letter literals,
 * lexeme holds the processed value (with escape sequences resolved).
 */
struct Token {
    TokenType type;
    std::string lexeme;
    int line;
    int column;
};

/**
 * @brief Represents a lexical error encountered during scanning.
 *
 * Never halts the lexer prematurely; stored in Lexer::errors_ for reporting.
 */
struct LexError {
    int line;
    int column;
    std::string message;
};

/**
 * @brief Converts a TokenType enum value to its human-readable string representation.
 */
inline std::string tokenTypeToString(TokenType type) {
    switch (type) {
        // Keywords: Types
        case TokenType::KW_NUM:         return "KW_NUM";
        case TokenType::KW_DECIMAL:     return "KW_DECIMAL";
        case TokenType::KW_LETTER:      return "KW_LETTER";
        case TokenType::KW_TEXT:        return "KW_TEXT";
        case TokenType::KW_FLAG:        return "KW_FLAG";
        case TokenType::KW_NOTHING:     return "KW_NOTHING";
        case TokenType::KW_FIXED:       return "KW_FIXED";
        case TokenType::KW_LET:         return "KW_LET";

        // Keywords: Values
        case TokenType::KW_YES:         return "KW_YES";
        case TokenType::KW_NO:          return "KW_NO";

        // Keywords: I/O
        case TokenType::KW_OUT:         return "KW_OUT";
        case TokenType::KW_IN:          return "KW_IN";

        // Keywords: Control Flow
        case TokenType::KW_IF:          return "KW_IF";
        case TokenType::KW_OTHER:       return "KW_OTHER";
        case TokenType::KW_WHILE:       return "KW_WHILE";
        case TokenType::KW_REP:         return "KW_REP";
        case TokenType::KW_STOP:        return "KW_STOP";
        case TokenType::KW_SKIP:        return "KW_SKIP";
        case TokenType::KW_END:         return "KW_END";

        // Keywords: Functions
        case TokenType::KW_FUNC:        return "KW_FUNC";
        case TokenType::KW_START:       return "KW_START";

        // Identifiers and Literals
        case TokenType::IDENTIFIER:     return "IDENTIFIER";
        case TokenType::NUM_LITERAL:    return "NUM_LITERAL";
        case TokenType::DECIMAL_LITERAL:return "DECIMAL_LITERAL";
        case TokenType::TEXT_LITERAL:   return "TEXT_LITERAL";
        case TokenType::LETTER_LITERAL: return "LETTER_LITERAL";

        // Operators
        case TokenType::PLUS:           return "PLUS";
        case TokenType::MINUS:          return "MINUS";
        case TokenType::STAR:           return "STAR";
        case TokenType::SLASH:          return "SLASH";
        case TokenType::PERCENT:        return "PERCENT";
        case TokenType::ASSIGN:         return "ASSIGN";
        case TokenType::PLUS_ASSIGN:    return "PLUS_ASSIGN";
        case TokenType::MINUS_ASSIGN:   return "MINUS_ASSIGN";
        case TokenType::STAR_ASSIGN:    return "STAR_ASSIGN";
        case TokenType::SLASH_ASSIGN:   return "SLASH_ASSIGN";
        case TokenType::INCREMENT:      return "INCREMENT";
        case TokenType::DECREMENT:      return "DECREMENT";
        case TokenType::EQUAL_EQUAL:    return "EQUAL_EQUAL";
        case TokenType::BANG_EQUAL:     return "BANG_EQUAL";
        case TokenType::LESS:           return "LESS";
        case TokenType::GREATER:        return "GREATER";
        case TokenType::LESS_EQUAL:     return "LESS_EQUAL";
        case TokenType::GREATER_EQUAL:  return "GREATER_EQUAL";
        case TokenType::AND_AND:        return "AND_AND";
        case TokenType::OR_OR:          return "OR_OR";
        case TokenType::BANG:           return "BANG";

        // Punctuation
        case TokenType::SEMICOLON:      return "SEMICOLON";
        case TokenType::COMMA:          return "COMMA";
        case TokenType::LEFT_PAREN:     return "LEFT_PAREN";
        case TokenType::RIGHT_PAREN:    return "RIGHT_PAREN";
        case TokenType::LEFT_BRACE:     return "LEFT_BRACE";
        case TokenType::RIGHT_BRACE:    return "RIGHT_BRACE";
        case TokenType::LEFT_BRACKET:   return "LEFT_BRACKET";
        case TokenType::RIGHT_BRACKET:  return "RIGHT_BRACKET";

        // End of File
        case TokenType::TOKEN_EOF:      return "TOKEN_EOF";
    }
    return "UNKNOWN";
}

#endif // YALL_TOKEN_H
