#ifndef YALL_SYMBOL_TABLE_H
#define YALL_SYMBOL_TABLE_H

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <iostream>
#include "types.h"

/**
 * @enum SymbolKind
 * @brief Categorizes the role of a declared symbol.
 */
enum class SymbolKind {
    VARIABLE,
    PARAMETER,
    FUNCTION
};

inline std::string symbolKindToString(SymbolKind kind) {
    switch (kind) {
        case SymbolKind::VARIABLE:  return "variable";
        case SymbolKind::PARAMETER: return "parameter";
        case SymbolKind::FUNCTION:  return "function";
    }
    return "unknown";
}

/**
 * @struct Symbol
 * @brief Records metadata about a declared identifier.
 */
struct Symbol {
    std::string name;
    std::string uniqueName;
    SymbolKind kind;
    Type type;
    bool isFixed = false;
    int declLine = 0;
    int declColumn = 0;
    bool used = false;     // True if read in an expression
    bool written = false;  // True if assigned / modified

    // Function-specific metadata
    Type returnType = Type::NOTHING;
    std::vector<Type> paramTypes;
    std::vector<std::string> paramNames;
};

/**
 * @struct ScopeRecord
 * @brief Logged record of a scope preserved for reporting after scopes are popped.
 */
struct ScopeRecord {
    std::string name; // Heading name, e.g. "Global Scope", "Function square", "Block (Line 4)"
    int depth = 0;
    std::vector<Symbol> symbols;
};

/**
 * @class SymbolTable
 * @brief Manages scoped identifier bindings and lifetime during semantic analysis.
 */
class SymbolTable {
public:
    SymbolTable();

    // Scope management
    void enterScope(const std::string& scopeName);
    void exitScope();

    // Symbol declaration
    bool declareVariable(const std::string& name, Type type, bool isFixed,
                         int line, int col, SymbolKind kind, std::string& outUniqueName);
    bool declareFunction(const std::string& name, Type returnType,
                         const std::vector<Type>& paramTypes,
                         const std::vector<std::string>& paramNames,
                         int line, int col);

    // Symbol lookup
    Symbol* lookupVariable(const std::string& name);
    Symbol* lookupVariableCurrentScope(const std::string& name);
    Symbol* lookupFunction(const std::string& name);

    // Unique name generation
    std::string generateUniqueName(const std::string& name);

    // Scope logging & printing
    const std::vector<std::shared_ptr<ScopeRecord>>& getScopeLogs() const;
    void printTable(std::ostream& out = std::cout) const;

    // Check for unused local variables in all finished scopes
    struct UnusedVar {
        std::string name;
        int line;
        int col;
    };
    std::vector<UnusedVar> getUnusedVariables() const;

private:
    // Scope representation
    struct Scope {
        std::shared_ptr<ScopeRecord> logRecord;
        std::unordered_map<std::string, Symbol> symbols;
    };

    std::vector<Scope> scopeStack_;
    std::unordered_map<std::string, Symbol> functions_;
    std::vector<std::shared_ptr<ScopeRecord>> allScopeLogs_;

    // Counter for unique names across the program
    std::unordered_map<std::string, int> nameCounts_;
};

#endif // YALL_SYMBOL_TABLE_H
