#ifndef YALL_LEXER_H
#define YALL_LEXER_H

#include <string>
#include <vector>
#include <unordered_map>
#include "token.h"

/**
 * @class Lexer
 * @brief Performs lexical analysis (scanning) for the Yall programming language.
 *
 * Converts a sequence of raw source characters into a stream of Tokens.
 * Features:
 *  - Longest-match operator scanning
 *  - Escape sequence handling in string and letter literals
 *  - Decimal and integer literal validation (rejecting malformed "5." and ".5")
 *  - Single-line and multi-line comments
 *  - C-keyword diagnostic suggestions
 *  - Non-stopping error recovery (collecting all errors into an error list)
 */
class Lexer {
public:
    /**
     * @brief Constructs a new Lexer with the provided source code.
     * @param source Raw source code string.
     */
    explicit Lexer(const std::string& source);

    /**
     * @brief Scans the entire source and produces a vector of Tokens.
     * Always appends a final TOKEN_EOF token.
     * @return std::vector<Token> Stream of successfully scanned tokens.
     */
    std::vector<Token> tokenize();

    /**
     * @brief Returns all lexical errors encountered during tokenization.
     * @return const std::vector<LexError>& List of errors.
     */
    const std::vector<LexError>& errors() const;

private:
    // Source buffer and position tracking
    std::string source_;
    size_t current_ = 0;    // Index of the next character to consume
    int line_ = 1;          // Current line number (1-based)
    int column_ = 1;        // Current column number (1-based)

    // Token start tracking (for recording the accurate start location of tokens)
    int startLine_ = 1;
    int startCol_ = 1;
    size_t startIndex_ = 0;

    // Collected tokens and errors
    std::vector<Token> tokens_;
    std::vector<LexError> errors_;

    // Lookup tables for keywords and C suggestions
    static const std::unordered_map<std::string, TokenType> keywords_;
    static const std::unordered_map<std::string, std::string> cSuggestions_;

    // Core scanner helpers (as required by specification)
    char peek() const;
    char peekNext() const;
    char advance();
    bool isAtEnd() const;

    // Token and error emission helpers
    void scanToken();
    void addToken(TokenType type, const std::string& lexeme);
    void addToken(TokenType type);
    void addError(int line, int col, const std::string& message);

    // Specialized lexical handlers
    void scanIdentifierOrKeyword();
    void scanNumber();
    void scanString();
    void scanLetter();
    void skipWhitespaceAndComments();
};

#endif // YALL_LEXER_H
