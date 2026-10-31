#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <iomanip>
#include "token.h"
#include "lexer.h"
#include "ast.h"
#include "parser.h"
#include "ast_printer.h"
#include "types.h"
#include "symbol_table.h"
#include "semantic.h"
#include "numeric.h"
#include "tac.h"
#include "tac_printer.h"
#include "irgen.h"
#include "optimizer.h"
#include "bytecode.h"
#include "codegen.h"
#include "disassembler.h"
#include "vm.h"
#include "diagnostics.h"

/**
 * @brief Checks if a string ends with a specific suffix.
 */
static bool endsWith(const std::string& str, const std::string& suffix) {
    if (str.length() < suffix.length()) return false;
    return str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
}

static void printHelp() {
    std::cout << "Usage: ./yall <file.yall> [options]\n\n"
              << "Options:\n"
              << "  (no options)     Compile and execute program on the Virtual Machine\n"
              << "  --check          Run front-end validation only (Lex, Parse, Semantic)\n"
              << "  --tokens         Dump Phase 1 token stream\n"
              << "  --ast            Dump Phase 2 Abstract Syntax Tree\n"
              << "  --semantics      Dump Phase 3 Symbol Table and Typed AST\n"
              << "  --tac            Dump Phase 4 Three-Address Code (raw TAC)\n"
              << "  --opt            Dump Phase 5 Optimized TAC & statistics table\n"
              << "  --bytecode       Dump Phase 6 Bytecode Disassembly\n"
              << "  --run            Force execution on the Virtual Machine\n"
              << "  --all            Run all compiler phases in sequence with banners\n"
              << "  --no-opt         Disable optimizations (bytecode uses raw TAC)\n"
              << "  --trace          Print instruction-level trace to stderr during execution\n"
              << "  --max-steps <N>  Halt VM after N instructions with runtime error\n"
              << "  --help           Display this help manual\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printHelp();
        return 2;
    }

    std::string filename;
    bool showTokens = false;
    bool showAST = false;
    bool showSemantics = false;
    bool showTAC = false;
    bool showOpt = false;
    bool showBytecode = false;
    bool runVM = false;
    bool runCheck = false;
    bool runAll = false;
    bool noOpt = false;
    bool traceVM = false;
    int64_t maxSteps = 0;

    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help") {
            printHelp();
            return 0;
        } else if (arg == "--check") {
            runCheck = true;
        } else if (arg == "--all") {
            runAll = true;
        } else if (arg == "--tokens") {
            showTokens = true;
        } else if (arg == "--ast") {
            showAST = true;
        } else if (arg == "--semantics") {
            showSemantics = true;
        } else if (arg == "--tac") {
            showTAC = true;
        } else if (arg == "--opt") {
            showOpt = true;
        } else if (arg == "--bytecode") {
            showBytecode = true;
        } else if (arg == "--run") {
            runVM = true;
        } else if (arg == "--no-opt") {
            noOpt = true;
        } else if (arg == "--trace") {
            traceVM = true;
            runVM = true;
        } else if (arg == "--max-steps") {
            if (i + 1 < argc) {
                try {
                    maxSteps = std::stoll(argv[++i]);
                } catch (...) {
                    std::cerr << "Error: Invalid number for --max-steps\n";
                    return 2;
                }
            } else {
                std::cerr << "Error: Missing argument for --max-steps\n";
                return 2;
            }
            runVM = true;
        } else if (!arg.empty() && arg[0] == '-') {
            std::cerr << "Error: Unrecognized option '" << arg << "'\n";
            std::cerr << "Use --help for a list of valid options.\n";
            return 2;
        } else if (filename.empty()) {
            filename = arg;
        } else {
            std::cerr << "Error: Multiple input files specified ('" << filename << "' and '" << arg << "')\n";
            return 2;
        }
    }

    if (filename.empty()) {
        std::cerr << "Error: No input file specified.\n";
        return 2;
    }

    if (!endsWith(filename, ".yall")) {
        std::cerr << "Warning: File '" << filename << "' does not have a .yall extension.\n";
    }

    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file '" << filename << "'\n";
        return 2;
    }

    std::string source((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();

    bool hasDumpFlag = showTokens || showAST || showSemantics || showTAC || showOpt || showBytecode;
    if (!hasDumpFlag && !runCheck && !runAll) {
        runVM = true; // Default execution when no dump flags are passed
    }

    // ========================================================================
    // Pipeline Stage 1: Lexical Analysis
    // ========================================================================
    if (runAll) {
        std::cout << "=== PHASE 1: LEXICAL ANALYSIS (tokens) ===\n";
    }

    Lexer lexer(source);
    std::vector<Token> tokens = lexer.tokenize();
    const std::vector<LexError>& lexErrors = lexer.errors();

    if (showTokens || runAll) {
        std::cout << std::left
                  << std::setw(8)  << "LINE"
                  << std::setw(8)  << "COL"
                  << std::setw(20) << "TYPE"
                  << "LEXEME" << "\n";
        std::cout << std::string(60, '-') << "\n";

        for (const auto& tok : tokens) {
            std::cout << std::left
                      << std::setw(8)  << tok.line
                      << std::setw(8)  << tok.column
                      << std::setw(20) << tokenTypeToString(tok.type)
                      << tok.lexeme << "\n";
        }
    }

    if (!lexErrors.empty()) {
        for (const auto& err : lexErrors) {
            Diagnostics::report(std::cout, source, err.line, err.column, err.message, "[lexical]");
        }
        std::cout << tokens.size() << " tokens, " << lexErrors.size() << " errors\n";
        std::cout << "fix lexical errors first\n";
        if (runAll) {
            std::cout << "Compilation stopped at Phase 1: Lexical Analysis\n";
        }
        return 1;
    }

    if (showTokens && !showAST && !showSemantics && !showTAC && !showOpt && !showBytecode && !runVM && !runAll) {
        std::cout << tokens.size() << " tokens, 0 errors\n";
        return 0;
    }

    // ========================================================================
    // Pipeline Stage 2: Syntax Analysis (Parser)
    // ========================================================================
    if (runAll) {
        std::cout << "\n=== PHASE 2: SYNTAX ANALYSIS (AST) ===\n";
    }

    Parser parser(std::move(tokens));
    std::unique_ptr<ProgramNode> ast = parser.parse();
    const std::vector<ParseError>& parseErrors = parser.errors();

    if (!parseErrors.empty()) {
        for (const auto& err : parseErrors) {
            Diagnostics::report(std::cout, source, err.line, err.column, err.message, "[syntax]");
        }
        std::cout << parseErrors.size() << " parse errors\n";
        if (runAll) {
            std::cout << "Compilation stopped at Phase 2: Syntax Analysis\n";
        }
        return 1;
    }

    if (showAST && !runAll && !showSemantics && !showTAC && !showOpt && !showBytecode && !runVM) {
        if (ast) {
            ASTPrinter::print(ast.get(), std::cout, /*showTypes=*/false);
        }
        return 0;
    }

    if (runAll && ast) {
        ASTPrinter::print(ast.get(), std::cout, /*showTypes=*/false);
    }

    // ========================================================================
    // Pipeline Stage 3: Semantic Analysis
    // ========================================================================
    if (runAll) {
        std::cout << "\n=== PHASE 3: SEMANTIC ANALYSIS (Symbol Table & Typed AST) ===\n";
    }

    SemanticAnalyzer analyzer;
    analyzer.analyze(ast.get());

    const auto& semErrors = analyzer.errors();
    const auto& semWarnings = analyzer.warnings();

    if (showSemantics || runAll) {
        analyzer.symbolTable().printTable(std::cout);
        if (ast) {
            std::cout << "\n=== TYPED AST ===\n";
            ASTPrinter::print(ast.get(), std::cout, /*showTypes=*/true);
            std::cout << "\n";
        }
    }

    for (const auto& w : semWarnings) {
        Diagnostics::report(std::cout, source, w.line, w.column, w.message, "[semantic]", true);
    }

    for (const auto& err : semErrors) {
        Diagnostics::report(std::cout, source, err.line, err.column, err.message, "[semantic]", false);
    }

    if (showSemantics || (!semErrors.empty() && !showTAC && !showOpt && !showBytecode && !runVM)) {
        std::cout << semErrors.size() << " semantic errors, " << semWarnings.size() << " warnings\n";
    }

    if (!semErrors.empty()) {
        if (runAll) {
            std::cout << "Compilation stopped at Phase 3: Semantic Analysis\n";
        }
        return 1;
    }

    if (runCheck) {
        std::cout << "All front-end checks passed successfully (0 errors, "
                  << semWarnings.size() << " warnings).\n";
        return 0;
    }

    if (showSemantics && !runAll && !showTAC && !showOpt && !showBytecode && !runVM) {
        return 0;
    }

    // ========================================================================
    // Pipeline Stage 4: Three-Address Code (TAC) Generation
    // ========================================================================
    if (runAll) {
        std::cout << "\n=== PHASE 4: THREE-ADDRESS CODE GENERATION (raw TAC) ===\n";
    }

    IRGen irgen;
    TACProgram rawTac = irgen.generate(ast.get());

    if (showTAC || runAll) {
        TACPrinter::print(rawTac, std::cout);
        if (showTAC && !runAll && !showOpt && !showBytecode && !runVM) {
            return 0;
        }
    }

    // ========================================================================
    // Pipeline Stage 5: TAC Optimizer
    // ========================================================================
    if (runAll) {
        std::cout << "\n=== PHASE 5: TAC OPTIMIZATION ===\n";
    }

    TACProgram optTac = rawTac;
    TACOptimizer optimizer;
    std::vector<OptStats> stats;
    if (!noOpt) {
        stats = optimizer.optimize(optTac);
    }

    if (showOpt || runAll) {
        TACPrinter::print(optTac, std::cout);
        if (!noOpt) {
            TACOptimizer::printStatsTable(stats, std::cout);
        }
        if (showOpt && !runAll && !showBytecode && !runVM) {
            return 0;
        }
    }

    // ========================================================================
    // Pipeline Stage 6: Bytecode Generation
    // ========================================================================
    if (runAll) {
        std::cout << "\n=== PHASE 6: BYTECODE GENERATION ===\n";
    }

    BytecodeGen codegen;
    const TACProgram& tacToTranslate = noOpt ? rawTac : optTac;
    BCProgram bcProg = codegen.generate(tacToTranslate);

    if (showBytecode || runAll) {
        Disassembler::disassemble(bcProg, std::cout);
        if (showBytecode && !runAll && !runVM) {
            return 0;
        }
    }

    // ========================================================================
    // Pipeline Stage 7: Execution (VM)
    // ========================================================================
    if (runVM || runAll) {
        if (runAll) {
            std::cout << "\n=== PHASE 7: EXECUTION ===\n";
        }
        VM vm(bcProg);
        return vm.run(traceVM, maxSteps);
    }

    return 0;
}
