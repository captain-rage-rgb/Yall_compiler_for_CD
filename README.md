# The Yall Programming Language & Compiler

Yall is a statically-typed, procedural programming language designed for clarity, safety, and educational transparency. This project contains a complete, industrial-quality end-to-end compiler for Yall written in pure C++17 (using only standard library facilities), spanning all 8 traditional compiler phases from lexical analysis to stack-based bytecode virtual machine execution.

---

## 1. Keyword Reference & C Equivalents

| Yall Keyword | C Equivalent | Purpose / Notes |
| :--- | :--- | :--- |
| `num` | `int64_t` | 64-bit signed integer |
| `decimal` | `double` | IEEE 754 double precision float |
| `letter` | `char` | Single ASCII character |
| `text` | `char*` / `std::string` | String literal or dynamically allocated string handle |
| `flag` | `bool` (`_Bool`) | Boolean logic (`yes` / `no`) |
| `nothing` | `void` | Return type for functions returning no value |
| `fixed` | `const` | Constant declaration (must have initializer; cannot be reassigned) |
| `let` | `auto` | Type-inferred local variable declaration (must have initializer) |
| `yes` | `true` | Flag literal true |
| `no` | `false` | Flag literal false |
| `if` | `if` | Conditional branching |
| `other` | `else` | Alternative branch (`other if` for `else if`) |
| `till` | `while` | Condition-controlled loop |
| `rep` | `for` | Three-part repetition loop: `rep (init; cond; update) { ... }` |
| `stop` | `break` | Terminate nearest enclosing loop |
| `skip` | `continue` | Jump to next iteration of nearest enclosing loop |
| `func` | function declaration | Defines a function: `func <type> <name>(<params>) { ... }` |
| `start` | `main` | Mandatory entry-point function: `func start() { ... }` |
| `end` | `return` | Function return statement (`end;` or `end <expr>;`) |
| `out` | `printf` | Standard formatted output |
| `in` | `fgets` / `getline` | Standard line input (returns `text`) |

---

## 2. Complete Grammar (EBNF)

```ebnf
Program        ::= { FunctionDecl }
FunctionDecl   ::= "func" ( TypeName | "nothing" ) IDENTIFIER "(" [ ParamList ] ")" Block
                 | "func" "start" "(" ")" Block

ParamList      ::= Param { "," Param }
Param          ::= TypeName IDENTIFIER

Block          ::= "{" { Statement } "}"
Statement      ::= VarDecl
                 | AssignStmt
                 | IncDecStmt
                 | OutStmt
                 | IfStmt
                 | WhileStmt
                 | RepStmt
                 | StopStmt
                 | SkipStmt
                 | EndStmt
                 | CallStmt
                 | Block

VarDecl        ::= "let" IDENTIFIER "=" Expression ";"
                 | [ "fixed" ] TypeName IDENTIFIER [ "=" Expression ] ";"

AssignStmt     ::= IDENTIFIER ( "=" | "+=" | "-=" | "*=" | "/=" ) Expression ";"
IncDecStmt     ::= ( "++" | "--" ) IDENTIFIER ";"
                 | IDENTIFIER ( "++" | "--" ) ";"

OutStmt        ::= "out" "(" [ Expression { "," Expression } ] ")" ";"
IfStmt         ::= "if" "(" Expression ")" Block [ "other" ( IfStmt | Block ) ]
WhileStmt      ::= "till" "(" Expression ")" Block
RepStmt        ::= "rep" "(" [ ( VarDecl | AssignStmt ) ] ";" [ Expression ] ";" [ ( IncDecStmt | AssignStmt ) ] ")" Block

StopStmt       ::= "stop" ";"
SkipStmt       ::= "skip" ";"
EndStmt        ::= "end" [ Expression ] ";"
CallStmt       ::= IDENTIFIER "(" [ ArgList ] ")" ";"

Expression     ::= LogicOr
LogicOr        ::= LogicAnd { "||" LogicAnd }
LogicAnd       ::= Equality { "&&" Equality }
Equality       ::= Relational { ( "==" | "!=" ) Relational }
Relational     ::= Additive { ( "<" | "<=" | ">" | ">=" ) Additive }
Additive       ::= Multiplicative { ( "+" | "-" ) Multiplicative }
Multiplicative ::= Unary { ( "*" | "/" | "%" ) Unary }
Unary          ::= ( "-" | "!" ) Unary | Primary
Primary        ::= NUM_LITERAL | DECIMAL_LITERAL | TEXT_LITERAL | LETTER_LITERAL
                 | "yes" | "no" | IDENTIFIER | "(" Expression ")"
                 | IDENTIFIER "(" [ ArgList ] ")"
                 | "in" "(" [ Expression ] ")"

ArgList        ::= Expression { "," Expression }
TypeName       ::= "num" | "decimal" | "letter" | "text" | "flag"
```

---

## 3. Types and Conversions

1. **Primitive Types**:
   - `num`: 64-bit signed two's complement integer (`int64_t`).
   - `decimal`: IEEE 754 double precision floating point (`double`).
   - `letter`: Single ASCII character (`char`).
   - `text`: String of characters.
   - `flag`: Boolean truth value (`yes` / `no`).
   - `nothing`: Return type for functions returning no value.
   - `error`: Sentinel type used by the compiler to suppress cascading errors.

2. **Implicit Widening Rule (Rule 6)**:
   - The **only** implicit type conversion permitted in Yall is `num -> decimal` (integer widening).
   - `decimal -> num` is strictly illegal and produces: `"cannot convert decimal to num without loss of data"`.
   - String concatenation via `+` is disallowed (hints: `"text concatenation is not supported"`).

---

## 4. Input & Output System

- **`out(...)`**: Takes zero or more arguments of any type except `nothing`.
  - Arguments are evaluated left-to-right.
  - A space separator is automatically printed between consecutive arguments.
  - A newline is automatically printed after the last argument.
  - `out();` prints only a newline.
- **`in([prompt])`**: Reads an entire line from standard input.
  - Optional prompt expression of type `text` is printed without a trailing newline.
  - Returns a value of type `text`.
  - Strips carriage returns (`\r`) automatically for cross-platform compatibility.

---

## 5. Compiler Architecture & Pipeline

```
Source (.yall) ──> [Lexer] ──> [Parser] ──> [Semantic Analyzer] ──> [IR Gen] ──> [Optimizer] ──> [Bytecode Gen] ──> [Virtual Machine]
```

- **Phase 1 (Lexer)**: Tokenizes raw source into typed tokens while tracking 1-indexed lines and columns.
- **Phase 2 (Parser)**: Builds an Abstract Syntax Tree (AST) using recursive-descent parsing with panic-mode recovery.
- **Phase 3 (Semantic Analyzer)**: Enforces scoping, unique renaming (`x_2`), type rules, loop depths, and return path checks across two passes.
- **Phase 4 (Three-Address Code)**: Flattens the typed AST into linearized Three-Address Code with explicit widening (`I2D`) and short-circuit branches.
- **Phase 5 (TAC Optimizer)**: Applies iterative local constant folding, copy propagation, algebraic simplifications, jump threading, and dead code elimination.
- **Phase 6 (Bytecode Generator)**: Translates TAC into stack-machine instructions with slot assignment and deduplicated constant pools.
- **Phase 7 (Virtual Machine)**: Executes bytecode using a heap-based frame stack, typed cell operations, and Python-like shortest round-trip float formatting.
- **Phase 8 (Diagnostics & Quality)**: Provides caret-annotated error reporting, differential testing, and complete documentation.

---

## 6. Command-Line Interface & Exit Codes

### Usage
```bash
./yall <file.yall> [options]
```

### Options
- `(no options)`: Compiles and executes the program on the Virtual Machine.
- `--check`: Runs front-end analysis only (Lexer, Parser, Semantic Analyzer).
- `--tokens`: Dumps the token table from Phase 1.
- `--ast`: Dumps the indented Abstract Syntax Tree from Phase 2.
- `--semantics`: Dumps the Scoped Symbol Table and Typed AST from Phase 3.
- `--tac`: Dumps raw Three-Address Code from Phase 4.
- `--opt`: Dumps optimized Three-Address Code and optimization statistics from Phase 5.
- `--bytecode`: Dumps disassembled bytecode instructions from Phase 6.
- `--run`: Forces VM execution (default when no dump flags are given).
- `--all`: Runs every phase in order under clear section banners.
- `--no-opt`: Disables TAC optimizations (passes raw TAC to bytecode generator and VM).
- `--trace`: Prints an instruction execution trace and stack top to stderr during VM execution.
- `--max-steps <N>`: Halts execution with runtime error after `N` VM steps (guards against infinite loops).
- `--help`: Displays the help manual.

### Exit Codes
- `0`: Successful execution / compilation.
- `1`: Compile-time error (lexical, syntax, or semantic).
- `2`: File I/O error or invalid command-line invocation.
- `3`: Runtime error in the Virtual Machine (e.g. division by zero, stack overflow).

---

## 7. Optimizations Performed (Phase 5)

1. **Local Constant Folding**:
   - Arithmetic on `num` constants (with two's complement wrapping).
   - Safe division/modulo folding (guards against division by constant zero or `INT64_MIN / -1`).
   - Decimal arithmetic constant folding (never folds `0.0` divisor).
   - Comparison and boolean constant evaluation.
2. **Copy & Constant Propagation**:
   - Tracks known replacements forward through basic blocks; clears knowledge at control labels.
3. **Algebraic Simplification (for `num`)**:
   - `x + 0 -> x`, `0 + x -> x`, `x - 0 -> x`
   - `x * 1 -> x`, `1 * x -> x`, `x * 0 -> 0`, `0 * x -> 0`
   - `x / 1 -> x`
4. **Branch Folding**:
   - `IF_FALSE yes GOTO L` is eliminated.
   - `IF_FALSE no GOTO L` becomes unconditional `GOTO L`.
5. **Temp Forwarding**:
   - When a temporary is assigned once and read once in `v = COPY t`, the producing instruction is rewritten to write directly to `v`.
6. **Control-Flow Cleanup**:
   - Unreachable instruction elimination after unconditional jumps or returns.
   - Jump threading (following chains of jumps).
   - Deletion of jumps to the immediately following label (`GOTO L; L:`).
   - Elimination of unreferenced labels.
7. **Dead Code Elimination**:
   - Removal of pure operations and copy assignments to unused variables or temporaries.

---

## 8. Bytecode Instruction Set

| Instruction | Operand | Description |
| :--- | :--- | :--- |
| `PUSH_CONST` | `k` (const index) | Push constant from constant pool onto operand stack |
| `LOAD` | `s` (slot index) | Push value from local frame slot `s` |
| `STORE` | `s` (slot index) | Pop value and store into local frame slot `s` |
| `POP` | none | Discard top operand |
| `ADD_I / SUB_I / MUL_I` | none | Pop 2 integers, compute wrapped op, push result |
| `DIV_I / MOD_I` | none | Pop 2 integers, safe divide / modulo, push result |
| `NEG_I` | none | Pop integer, compute wrapped negation, push result |
| `ADD_D / SUB_D / MUL_D` | none | Pop 2 doubles, compute IEEE op, push result |
| `DIV_D` | none | Pop 2 doubles, check non-zero divisor, push result |
| `NEG_D` | none | Pop double, negate, push result |
| `I2D` | none | Pop integer, widen to double, push result |
| `LT_I / LE_I / GT_I / GE_I / EQ_I / NE_I` | none | Pop 2 integers/chars/flags, compare, push 1 or 0 |
| `LT_D / LE_D / GT_D / GE_D / EQ_D / NE_D` | none | Pop 2 doubles, compare, push 1 or 0 |
| `EQ_S / NE_S` | none | Pop 2 string handles, compare contents, push 1 or 0 |
| `NOT` | none | Pop boolean integer, logical not, push 1 or 0 |
| `JMP` | `target` (PC) | Unconditional jump |
| `JMP_FALSE / JMP_TRUE` | `target` (PC) | Pop boolean, conditional jump |
| `CALL` | `f` (func index) | Call function `f`, pop parameters into callee slots |
| `RET` | none | Return from function |
| `RET_VAL` | none | Pop return value, return, push onto caller's stack |
| `PRINT_I / PRINT_D / PRINT_S / PRINT_L / PRINT_F` | none | Pop and print formatted value |
| `PRINT_SPACE / PRINT_NL` | none | Print single space / print newline |
| `INPUT / INPUT_PROMPT` | none | Read string from stdin and push string handle |
| `HALT` | none | Terminate virtual machine |

---

## 9. Known Limitations

- **No arrays, structs, or pointers**: Yall is an educational language focused on control flow, functions, and primitive scalar types.
- **No global variables**: All variables must reside inside function bodies or function parameters.
- **No text concatenation**: String combining with `+` is disallowed; users compose output via multiple arguments in `out()`.
- **No string-to-number parsing in language**: There is no standard library `parseInt` or `parseFloat` function in Yall.
- **Strings never freed in VM**: String handles allocated by input are kept in the VM string table for the duration of execution (standard arena model).
- **Single source file**: Yall programs compile from a single `.yall` file; there is no module or import system.

---

## 10. Building and Testing

```bash
# Build compiler
make

# Run complete 62-test verification suite
make test

# Build and run under Address & Undefined Behavior Sanitizers
make sanitize

# Clean build artifacts
make clean
```
