#ifndef YALL_AST_PRINTER_H
#define YALL_AST_PRINTER_H

#include <iostream>
#include "ast.h"

/**
 * @class ASTPrinter
 * @brief Prints an indented visual representation of the Yall Abstract Syntax Tree.
 *
 * Clearly displays operator precedence, control flow hierarchy, declarations,
 * and (when requested via --semantics) semantic type annotations on expressions.
 */
class ASTPrinter {
public:
    /**
     * @brief Prints the entire AST starting from the root node.
     * @param node The root AST node (usually ProgramNode).
     * @param out The output stream (defaults to std::cout).
     * @param showTypes If true, annotates expressions and declarations with their resolved types.
     */
    static void print(const ASTNode* node, std::ostream& out = std::cout, bool showTypes = false);

private:
    static void printNode(const ASTNode* node, int indent, std::ostream& out, bool showTypes);
    static void printExpression(const Expression* expr, int indent, std::ostream& out, bool showTypes);
    static void printStatement(const Statement* stmt, int indent, std::ostream& out, bool showTypes);
};

#endif // YALL_AST_PRINTER_H
