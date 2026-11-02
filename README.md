# Compiler C

**Compiler C** is a modular compiler written in **C**, designed as a learning and experimentation project for understanding how real-world compilers work — from lexical analysis to code generation.

---

## Overview

The project aims to implement a complete compilation pipeline for a simple C-like language:

1. **Lexical Analysis (Lexer)** – Converts the raw source code into a stream of tokens such as keywords, identifiers, literals, operators, and punctuators.  
2. **Syntax Analysis (Parser)** – Builds an Abstract Syntax Tree (AST) from the token stream, checking that the program follows the grammar rules.  
3. **Semantic Analysis** – Validates types, variable declarations, and scope rules.  
4. **Intermediate Code Generation** – Translates the AST into an intermediate representation (IR) suitable for optimization and code generation.  
5. **Code Optimization** – (Planned) Performs basic optimizations such as constant folding and dead-code elimination.  
6. **Target Code Generation** – (Planned) Produces executable assembly or machine code.

---

## Current Status

| Module | Status | Description |
|:--------|:--------:|:------------|
| **Lexer** | Done | Handles keywords, identifiers, literals, operators, and punctuators |
| **Parser** | Done | Recursive descent parser producing a typed Abstract Syntax Tree (AST) |
| **Semantic Analyzer** | Done | Scope management, symbol table, type checking, and validation |
| **IR Generator** | Done | Three-Address Code (TAC) intermediate representation generator |
| **Optimizer** | Planned | Basic code optimization |
| **Code Generator** | Planned | Produces target assembly or machine code |

---

## Supported Language Features

The lexer and parser recognize a rich subset of the C grammar:

### 1. Literals & Identifiers
- **Integer Literals**: e.g., `0`, `42`, `100`
- **Float Literals**: e.g., `3.14`, `0.5`
- **String Literals**: e.g., `"Hello, World!"` (with escape sequences)
- **Identifiers**: Standard C naming rules (letters, digits, underscores, starting with letter/underscore)

### 2. Data Types
- **Primitive Types**: `int`, `float`, `char`, `double`, `void`, `long`, `short`, `signed`, `unsigned`
- **Pointer Types**: e.g., `char*`, `int*`

### 3. Declarations
- **Variable Declarations**: Typed variables with optional initialization (`int x;`, `int x = 10;`, `char *msg = "hi";`)
- **Array Declarations**: Fixed-size arrays (`int arr[10];`, `int numbers[5];`)
- **Function Declarations & Prototypes**: Full function definitions with bodies and parameter lists (`int add(int a, int b) { ... }`, `void test(void);`)

### 4. Expressions & Operations
- **Binary Arithmetic**: `+`, `-`, `*`, `/`, `%`
- **Relational & Equality**: `<`, `<=`, `>`, `>=`, `==`, `!=`
- **Logical Operations**: `&&`, `||`
- **Unary Operations**:
  - Prefix: `+`, `-`, `!`, `~`, `++`, `--`, `&`, `*`
  - Postfix: `++`, `--`
- **Assignments**: `=`, `+=`, `-=`, `*=`, `/=` (right-associative)
- **Grouping**: Parenthesized expressions `(a + b) * c` respecting standard C precedence

### 5. Access Operations
- **Array Subscripting**: `arr[i]`, `matrix[row][col]`
- **Function Calls**: Calling functions with arbitrary argument lists (`foo(x, y + 1)`)

### 6. Control Flow Statements
- **Compound Statements**: `{ ... }` blocks with scoped, interleaved statements and declarations
- **Selection**: `if (condition) stmt` with optional `else stmt`
- **Loops**:
  - `while (condition) stmt`
  - `for (init; condition; increment) stmt`
- **Jump Statements**:
  - `return` (with or without expression)
  - `break`
  - `continue`
- **Expression & Empty Statements**: `x = 5;`, `;`

### 7. Comments & Formatting
- **Single-line comments**: `// ...`
- **Multi-line block comments**: `/* ... */`
- Delimiter and whitespace insensitivity

---

### 8. Intermediate Representation (IR)
- Linear **Three-Address Code (TAC)** representation using a doubly-linked instruction list
- Compiler-generated temporary variables (`t0`, `t1`, ...) for expression decomposition
- Label generation (`L0`, `L1`, ...) and control flow lowering (`if`, `ifFalse`, `goto`)
- Boolean short-circuit evaluation for `&&` and `||`
- Function call lowering (`arg`, `call`, `param`, `return`)
- Array allocation and indexing (`alloc`, `dest = arr[i]`, `arr[i] = src`)
- Loop context stack supporting arbitrarily nested `while` and `for` loops with `break` and `continue`

---

## Building and Running

Compile the compiler:
```bash
gcc -Wall -Wextra -pedantic main.c lexer.c parser.c semantic.c ir.c -o compiler
```

Run against a test source file:
```bash
./compiler test.txt
```
This prints:
1. The stream of lexed tokens.
2. The hierarchical Abstract Syntax Tree (AST).
3. The semantic analysis results and global symbol table.
4. The generated Three-Address Code (TAC) Intermediate Representation.
