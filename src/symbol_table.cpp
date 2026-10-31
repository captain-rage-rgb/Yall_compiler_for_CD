#include "symbol_table.h"
#include <iomanip>

SymbolTable::SymbolTable() {
    // Enter global scope upon initialization
    enterScope("Global Scope");
}

void SymbolTable::enterScope(const std::string& scopeName) {
    auto logRec = std::make_shared<ScopeRecord>();
    logRec->name = scopeName;
    logRec->depth = static_cast<int>(scopeStack_.size());

    allScopeLogs_.push_back(logRec);

    Scope s;
    s.logRecord = logRec;
    scopeStack_.push_back(std::move(s));
}

void SymbolTable::exitScope() {
    if (scopeStack_.empty()) return;

    // Update logRecord with the latest symbol state (including used flags)
    auto& currentScope = scopeStack_.back();
    currentScope.logRecord->symbols.clear();
    for (const auto& pair : currentScope.symbols) {
        currentScope.logRecord->symbols.push_back(pair.second);
    }

    scopeStack_.pop_back();
}

std::string SymbolTable::generateUniqueName(const std::string& name) {
    int& count = nameCounts_[name];
    if (count == 0) {
        count = 1;
        return name;
    }
    count++;
    return name + "_" + std::to_string(count);
}

bool SymbolTable::declareVariable(const std::string& name, Type type, bool isFixed,
                                  int line, int col, SymbolKind kind, std::string& outUniqueName) {
    if (scopeStack_.empty()) return false;

    auto& currentScope = scopeStack_.back();
    if (currentScope.symbols.find(name) != currentScope.symbols.end()) {
        return false; // Redeclaration in same scope is an error
    }

    outUniqueName = generateUniqueName(name);

    Symbol sym;
    sym.name = name;
    sym.uniqueName = outUniqueName;
    sym.kind = kind;
    sym.type = type;
    sym.isFixed = isFixed;
    sym.declLine = line;
    sym.declColumn = col;
    sym.used = false;
    sym.written = false;

    currentScope.symbols[name] = sym;
    currentScope.logRecord->symbols.push_back(sym);
    return true;
}

bool SymbolTable::declareFunction(const std::string& name, Type returnType,
                                  const std::vector<Type>& paramTypes,
                                  const std::vector<std::string>& paramNames,
                                  int line, int col) {
    if (functions_.find(name) != functions_.end()) {
        return false; // Duplicate function name
    }

    Symbol sym;
    sym.name = name;
    sym.uniqueName = name;
    sym.kind = SymbolKind::FUNCTION;
    sym.type = returnType;
    sym.isFixed = true;
    sym.declLine = line;
    sym.declColumn = col;
    sym.returnType = returnType;
    sym.paramTypes = paramTypes;
    sym.paramNames = paramNames;

    functions_[name] = sym;

    // Also record in Global Scope log
    if (!allScopeLogs_.empty()) {
        allScopeLogs_.front()->symbols.push_back(sym);
    }

    return true;
}

Symbol* SymbolTable::lookupVariable(const std::string& name) {
    // Search from innermost scope to outermost
    for (auto it = scopeStack_.rbegin(); it != scopeStack_.rend(); ++it) {
        auto found = it->symbols.find(name);
        if (found != it->symbols.end()) {
            return &(found->second);
        }
    }
    return nullptr;
}

Symbol* SymbolTable::lookupVariableCurrentScope(const std::string& name) {
    if (scopeStack_.empty()) return nullptr;
    auto& curr = scopeStack_.back();
    auto found = curr.symbols.find(name);
    if (found != curr.symbols.end()) {
        return &(found->second);
    }
    return nullptr;
}

Symbol* SymbolTable::lookupFunction(const std::string& name) {
    auto found = functions_.find(name);
    if (found != functions_.end()) {
        return &(found->second);
    }
    return nullptr;
}

const std::vector<std::shared_ptr<ScopeRecord>>& SymbolTable::getScopeLogs() const {
    return allScopeLogs_;
}

std::vector<SymbolTable::UnusedVar> SymbolTable::getUnusedVariables() const {
    std::vector<UnusedVar> unused;
    for (const auto& scope : allScopeLogs_) {
        for (const auto& sym : scope->symbols) {
            if (sym.kind == SymbolKind::VARIABLE && !sym.used) {
                unused.push_back(UnusedVar{sym.name, sym.declLine, sym.declColumn});
            }
        }
    }
    return unused;
}

void SymbolTable::printTable(std::ostream& out) const {
    out << "=== SYMBOL TABLE ===\n";
    for (const auto& scope : allScopeLogs_) {
        if (scope->symbols.empty() && scope->depth > 0) continue;

        std::string indentStr(scope->depth * 2, ' ');
        out << indentStr << "[" << scope->name << "]\n";

        for (const auto& sym : scope->symbols) {
            out << indentStr << "  ";
            if (sym.kind == SymbolKind::FUNCTION) {
                out << sym.name << " : func(";
                for (size_t i = 0; i < sym.paramTypes.size(); ++i) {
                    if (i > 0) out << ", ";
                    out << typeToString(sym.paramTypes[i]);
                }
                out << ") -> " << typeToString(sym.returnType);
                out << " (Line " << sym.declLine << ")\n";
            } else {
                out << sym.name;
                if (sym.uniqueName != sym.name) {
                    out << " [" << sym.uniqueName << "]";
                }
                out << " : ";
                if (sym.isFixed) out << "fixed ";
                out << symbolKindToString(sym.kind) << " " << typeToString(sym.type);
                out << " (Line " << sym.declLine << ")\n";
            }
        }
    }
    out << "\n";
}
