#include "lexer.h"
#include <cctype>

// ============================================================================
// Keywords Lookup Table
// ============================================================================
// Exact list of case-sensitive Yall keywords mapped to their specific TokenType.
const std::unordered_map<std::string, TokenType> Lexer::keywords_ = {
    // Types
    {"num",     TokenType::KW_NUM},
    {"decimal", TokenType::KW_DECIMAL},
    {"letter",  TokenType::KW_LETTER},
    {"text",    TokenType::KW_TEXT},
    {"flag",    TokenType::KW_FLAG},
    {"nothing", TokenType::KW_NOTHING},
    {"fixed",   TokenType::KW_FIXED},
    {"let",     TokenType::KW_LET},

    // Values
    {"yes",     TokenType::KW_YES},
    {"no",      TokenType::KW_NO},

    // I/O
    {"out",     TokenType::KW_OUT},
    {"in",      TokenType::KW_IN},

    // Control Flow
    {"if",      TokenType::KW_IF},
    {"other",   TokenType::KW_OTHER},
    {"till",    TokenType::KW_TILL},
    {"rep",     TokenType::KW_REP},
    {"stop",    TokenType::KW_STOP},
    {"skip",    TokenType::KW_SKIP},
    {"end",     TokenType::KW_END},

    // Functions
    {"func",    TokenType::KW_FUNC},
    {"start",   TokenType::KW_START}
};

// ============================================================================
// C-Keyword Diagnostic Suggestion Table
// ============================================================================
// If a user accidentally writes a C-style keyword, Yall guides them to the
// proper Yall keyword with a helpful diagnostic.
const std::unordered_map<std::string, std::string> Lexer::cSuggestions_ = {
    {"int",      "num"},
    {"long",     "num"},
    {"short",    "num"},
    {"unsigned", "num"},
    {"signed",   "num"},
    {"float",    "decimal"},
    {"double",   "decimal"},
    {"char",     "letter"},
    {"bool",     "flag"},
    {"void",     "nothing"},
    {"const",    "fixed"},
    {"true",     "yes"},
    {"false",    "no"},
    {"else",     "other"},
    {"while",    "till"},
    {"for",      "rep"},
    {"return",   "end"},
    {"printf",   "out"},
    {"scanf",    "in"}
};

// ============================================================================
// Constructor
// ============================================================================
Lexer::Lexer(const std::string& source) {
    // Normalize newlines (\r\n and \r -> \n) so column & line tracking is uniform
    source_.reserve(source.size());
    for (size_t i = 0; i < source.size(); ++i) {
        if (source[i] == '\r') {
            if (i + 1 < source.size() && source[i + 1] == '\n') {
                i++;
            }
            source_.push_back('\n');
        } else {
            source_.push_back(source[i]);
        }
    }
}

// ============================================================================
// Core Scanning API
// ============================================================================
std::vector<Token> Lexer::tokenize() {
    tokens_.clear();
    errors_.clear();
    current_ = 0;
    line_ = 1;
    column_ = 1;

    // Scan tokens until the end of source is reached
    while (!isAtEnd()) {
        startLine_ = line_;
        startCol_ = column_;
        startIndex_ = current_;
        scanToken();
    }

    // Per specification: Always append a final EOF token
    tokens_.push_back(Token{TokenType::TOKEN_EOF, "", line_, column_});
    return tokens_;
}

const std::vector<LexError>& Lexer::errors() const {
    return errors_;
}

// ============================================================================
// Character Inspection & Navigation Helpers
// ============================================================================
bool Lexer::isAtEnd() const {
    return current_ >= source_.length();
}

char Lexer::peek() const {
    if (isAtEnd()) return '\0';
    return source_[current_];
}

char Lexer::peekNext() const {
    if (current_ + 1 >= source_.length()) return '\0';
    return source_[current_ + 1];
}

char Lexer::advance() {
    if (isAtEnd()) return '\0';
    char c = source_[current_++];
    if (c == '\n') {
        line_++;
        column_ = 1;
    } else {
        column_++;
    }
    return c;
}

// ============================================================================
// Token & Error Recording Helpers
// ============================================================================
void Lexer::addToken(TokenType type, const std::string& lexeme) {
    tokens_.push_back(Token{type, lexeme, startLine_, startCol_});
}

void Lexer::addToken(TokenType type) {
    std::string lexeme = source_.substr(startIndex_, current_ - startIndex_);
    addToken(type, lexeme);
}

void Lexer::addError(int line, int col, const std::string& message) {
    errors_.push_back(LexError{line, col, message});
}

// ============================================================================
// Token Dispatcher
// ============================================================================
void Lexer::scanToken() {
    char c = advance();

    switch (c) {
        // --- Whitespace (Track line/column and skip) ---
        case ' ':
        case '\t':
        case '\n':
            // Already updated line_ and column_ in advance()
            break;

        // --- Single-Character Punctuation ---
        case '(': addToken(TokenType::LEFT_PAREN, "("); break;
        case ')': addToken(TokenType::RIGHT_PAREN, ")"); break;
        case '{': addToken(TokenType::LEFT_BRACE, "{"); break;
        case '}': addToken(TokenType::RIGHT_BRACE, "}"); break;
        case '[': addToken(TokenType::LEFT_BRACKET, "["); break;
        case ']': addToken(TokenType::RIGHT_BRACKET, "]"); break;
        case ',': addToken(TokenType::COMMA, ","); break;
        case ';': addToken(TokenType::SEMICOLON, ";"); break;
        case '%': addToken(TokenType::PERCENT, "%"); break;

        // --- Operators (with Longest Match) ---
        case '+':
            if (peek() == '+') {
                advance();
                addToken(TokenType::INCREMENT, "++");
            } else if (peek() == '=') {
                advance();
                addToken(TokenType::PLUS_ASSIGN, "+=");
            } else {
                addToken(TokenType::PLUS, "+");
            }
            break;

        case '-':
            if (peek() == '-') {
                advance();
                addToken(TokenType::DECREMENT, "--");
            } else if (peek() == '=') {
                advance();
                addToken(TokenType::MINUS_ASSIGN, "-=");
            } else {
                addToken(TokenType::MINUS, "-");
            }
            break;

        case '*':
            if (peek() == '=') {
                advance();
                addToken(TokenType::STAR_ASSIGN, "*=");
            } else {
                addToken(TokenType::STAR, "*");
            }
            break;

        case '/':
            if (peek() == '/') {
                // Single-line comment: skip until newline or EOF
                while (!isAtEnd() && peek() != '\n') {
                    advance();
                }
            } else if (peek() == '*') {
                // Multi-line comment: skip until */ or EOF
                int commentStartLine = startLine_;
                int commentStartCol = startCol_;
                advance(); // consume '*'
                bool closed = false;
                while (!isAtEnd()) {
                    if (peek() == '*' && peekNext() == '/') {
                        advance(); // consume '*'
                        advance(); // consume '/'
                        closed = true;
                        break;
                    }
                    advance();
                }
                if (!closed) {
                    addError(commentStartLine, commentStartCol,
                             "unterminated comment starting at line " + std::to_string(commentStartLine));
                }
            } else if (peek() == '=') {
                advance();
                addToken(TokenType::SLASH_ASSIGN, "/=");
            } else {
                addToken(TokenType::SLASH, "/");
            }
            break;

        case '=':
            if (peek() == '=') {
                advance();
                addToken(TokenType::EQUAL_EQUAL, "==");
            } else {
                addToken(TokenType::ASSIGN, "=");
            }
            break;

        case '!':
            if (peek() == '=') {
                advance();
                addToken(TokenType::BANG_EQUAL, "!=");
            } else {
                addToken(TokenType::BANG, "!");
            }
            break;

        case '<':
            if (peek() == '=') {
                advance();
                addToken(TokenType::LESS_EQUAL, "<=");
            } else {
                addToken(TokenType::LESS, "<");
            }
            break;

        case '>':
            if (peek() == '=') {
                advance();
                addToken(TokenType::GREATER_EQUAL, ">=");
            } else {
                addToken(TokenType::GREATER, ">");
            }
            break;

        case '&':
            if (peek() == '&') {
                advance();
                addToken(TokenType::AND_AND, "&&");
            } else {
                addError(startLine_, startCol_, "invalid character '&' (did you mean '&&'?)");
            }
            break;

        case '|':
            if (peek() == '|') {
                advance();
                addToken(TokenType::OR_OR, "||");
            } else {
                addError(startLine_, startCol_, "invalid character '|' (did you mean '||'?)");
            }
            break;

        // --- Decimal Literal starting with '.' (e.g. .5 is a malformed decimal) ---
        case '.':
            if (std::isdigit(static_cast<unsigned char>(peek()))) {
                // Malformed decimal: ".5"
                while (!isAtEnd() && std::isdigit(static_cast<unsigned char>(peek()))) {
                    advance();
                }
                addError(startLine_, startCol_, "malformed decimal literal");
            } else {
                addError(startLine_, startCol_, "invalid character '.'");
            }
            break;

        // --- Literals ---
        case '"':
            scanString();
            break;

        case '\'':
            scanLetter();
            break;

        default:
            if (std::isdigit(static_cast<unsigned char>(c))) {
                scanNumber();
            } else if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
                scanIdentifierOrKeyword();
            } else {
                addError(startLine_, startCol_, std::string("invalid character '") + c + "'");
            }
            break;
    }
}

// ============================================================================
// Identifier & Keyword Handler
// ============================================================================
void Lexer::scanIdentifierOrKeyword() {
    while (!isAtEnd() && (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_')) {
        advance();
    }

    std::string text = source_.substr(startIndex_, current_ - startIndex_);

    // 1. Check for standard Yall keywords
    auto kwIt = keywords_.find(text);
    if (kwIt != keywords_.end()) {
        addToken(kwIt->second, text);
        return;
    }

    // 2. Check for C keywords and provide friendly guidance
    auto cIt = cSuggestions_.find(text);
    if (cIt != cSuggestions_.end()) {
        addError(startLine_, startCol_,
                 "'" + text + "' is not a keyword in Yall. Did you mean '" + cIt->second + "'?");
        return;
    }

    // 3. Otherwise, it is a regular user identifier
    addToken(TokenType::IDENTIFIER, text);
}

// ============================================================================
// Number Literal Handler (Integer vs Decimal)
// ============================================================================
void Lexer::scanNumber() {
    while (!isAtEnd() && std::isdigit(static_cast<unsigned char>(peek()))) {
        advance();
    }

    // Check if followed by '.'
    if (peek() == '.') {
        // Lookahead to check if digits follow '.'
        if (std::isdigit(static_cast<unsigned char>(peekNext()))) {
            // Valid decimal literal (e.g. 3.14)
            advance(); // consume '.'
            while (!isAtEnd() && std::isdigit(static_cast<unsigned char>(peek()))) {
                advance();
            }
            std::string lexeme = source_.substr(startIndex_, current_ - startIndex_);
            addToken(TokenType::DECIMAL_LITERAL, lexeme);
        } else {
            // Malformed decimal literal (e.g. 5.)
            advance(); // consume '.'
            addError(startLine_, startCol_, "malformed decimal literal");
        }
    } else {
        // Valid integer literal (e.g. 42)
        std::string lexeme = source_.substr(startIndex_, current_ - startIndex_);
        addToken(TokenType::NUM_LITERAL, lexeme);
    }
}

// ============================================================================
// String Literal Handler
// ============================================================================
void Lexer::scanString() {
    std::string processed = "";
    bool hasEscapeError = false;

    while (!isAtEnd() && peek() != '"' && peek() != '\n') {
        if (peek() == '\\') {
            int escLine = line_;
            int escCol = column_;
            advance(); // consume '\\'

            if (isAtEnd() || peek() == '\n') {
                break; // Will trigger unterminated string check below
            }

            char esc = advance();
            if (esc == 'n') {
                processed += '\n';
            } else if (esc == 't') {
                processed += '\t';
            } else if (esc == '\\') {
                processed += '\\';
            } else if (esc == '"') {
                processed += '"';
            } else {
                addError(escLine, escCol, "unknown escape sequence");
                hasEscapeError = true;
            }
        } else {
            processed += advance();
        }
    }

    if (isAtEnd() || peek() == '\n') {
        addError(startLine_, startCol_, "unterminated string");
        return;
    }

    // Consume the closing quote '"'
    advance();

    // Only emit valid token if there were no lexical errors inside the string
    if (!hasEscapeError) {
        addToken(TokenType::TEXT_LITERAL, processed);
    }
}

// ============================================================================
// Letter Literal Handler
// ============================================================================
void Lexer::scanLetter() {
    // 1. Unterminated at end of line / EOF
    if (isAtEnd() || peek() == '\n') {
        addError(startLine_, startCol_, "unterminated letter literal");
        return;
    }

    // 2. Empty letter literal: ''
    if (peek() == '\'') {
        advance(); // consume closing '\''
        addError(startLine_, startCol_, "empty letter literal");
        return;
    }

    char resolved = '\0';
    bool hasEscapeError = false;

    // 3. Scan character or escape sequence
    if (peek() == '\\') {
        int escLine = line_;
        int escCol = column_;
        advance(); // consume '\\'

        if (isAtEnd() || peek() == '\n') {
            addError(startLine_, startCol_, "unterminated letter literal");
            return;
        }

        char esc = advance();
        if (esc == 'n') {
            resolved = '\n';
        } else if (esc == 't') {
            resolved = '\t';
        } else if (esc == '\\') {
            resolved = '\\';
        } else if (esc == '\'') {
            resolved = '\'';
        } else if (esc == '"') {
            resolved = '"';
        } else {
            addError(escLine, escCol, "unknown escape sequence");
            hasEscapeError = true;
        }
    } else {
        resolved = advance();
    }

    // 4. Exactly one character/escape must be followed by closing quote
    if (peek() == '\'') {
        advance(); // consume closing '\''
        if (!hasEscapeError) {
            addToken(TokenType::LETTER_LITERAL, std::string(1, resolved));
        }
        return;
    }

    // 5. If newline or EOF reached without closing quote
    if (isAtEnd() || peek() == '\n') {
        if (!hasEscapeError) {
            addError(startLine_, startCol_, "unterminated letter literal");
        }
        return;
    }

    // 6. Multi-character letter literal (e.g. 'ab')
    while (!isAtEnd() && peek() != '\'' && peek() != '\n') {
        advance();
    }
    if (peek() == '\'') {
        advance(); // consume closing quote
    }
    if (!hasEscapeError) {
        addError(startLine_, startCol_, "multi-character letter literal");
    }
}
