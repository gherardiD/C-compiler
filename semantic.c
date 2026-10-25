#define _POSIX_C_SOURCE 200809L
#include "semantic.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

// =======================================================================
// SEMANTIC ANALYZER IMPLEMENTATION
//
// In compiler design, Semantic Analysis bridges the gap between syntax
// (the grammar of the program, represented as the Abstract Syntax Tree)
// and target code generation.
//
// While the parser ensures statements like "int x = 5;" or "a + b" are
// grammatically valid, it does not know:
//   1. Whether "a" and "b" exist.
//   2. Whether "a" and "b" have types that can be added.
//   3. Whether "x" was already declared in the same scope.
//   4. Whether a "break" statement is actually located within a loop.
//
// The Semantic Analyzer answers these questions using:
//   - A Symbol Table with a stack of Scopes.
//   - Type inference and validation rules.
//   - Context tracking (e.g. current loop depth, function return type).
// =======================================================================

// =========== FORWARD DECLARATIONS ===========

static void analyzeStatement(SemanticAnalyzer *analyzer, ASTNode *node);
static char *analyzeExpression(SemanticAnalyzer *analyzer, ASTNode *node);
static int isLValue(SemanticAnalyzer *analyzer, ASTNode *node);

// =========== ERROR REPORTING ===========

// Prints a formatted semantic error message and increments the error counter.
static void semanticError(SemanticAnalyzer *analyzer, const char *format, ...) {
    analyzer->errorCount++;
    fprintf(stderr, "Semantic Error: ");
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fprintf(stderr, "\n");
}

// =========== TYPE SYSTEM UTILITIES ===========

// Checks if a type is a numeric primitive (int, float, char, double, long, short).
static int isNumericType(const char *type) {
    if (!type) return 0;
    return (strcmp(type, "int") == 0 ||
            strcmp(type, "float") == 0 ||
            strcmp(type, "char") == 0 ||
            strcmp(type, "double") == 0 ||
            strcmp(type, "long") == 0 ||
            strcmp(type, "short") == 0 ||
            strcmp(type, "unsigned") == 0 ||
            strcmp(type, "signed") == 0);
}

// Checks if a type is an integer-compatible type (can be used for array indexing, bitwise, etc.).
static int isIntegerType(const char *type) {
    if (!type) return 0;
    return (strcmp(type, "int") == 0 ||
            strcmp(type, "char") == 0 ||
            strcmp(type, "long") == 0 ||
            strcmp(type, "short") == 0 ||
            strcmp(type, "unsigned") == 0 ||
            strcmp(type, "signed") == 0);
}

// Checks if a type is a pointer (ends with '*').
static int isPointerType(const char *type) {
    if (!type) return 0;
    size_t len = strlen(type);
    return (len > 0 && type[len - 1] == '*');
}

// Strips one level of indirection: e.g. "int*" -> "int", "char**" -> "char*".
// Returns a dynamically allocated string.
static char *removePointer(const char *type) {
    if (!type) return strdup("unknown");
    size_t len = strlen(type);
    if (len > 0 && type[len - 1] == '*') {
        char *result = (char *)malloc(len);
        strncpy(result, type, len - 1);
        result[len - 1] = '\0';
        return result;
    }
    return strdup(type);
}

// Adds one level of pointer indirection: e.g. "int" -> "int*", "char*" -> "char**".
// Returns a dynamically allocated string.
static char *addPointer(const char *type) {
    if (!type) return strdup("unknown");
    size_t len = strlen(type);
    char *result = (char *)malloc(len + 2);
    sprintf(result, "%s*", type);
    return result;
}

// Checks if 'actual' type can be safely assigned or passed to an 'expected' type.
// In C:
//  - Identical types are compatible.
//  - Numeric types (int, float, char, etc.) can implicitly convert to each other.
//  - void* can implicitly convert to/from any pointer type.
//  - "unknown" is considered compatible to prevent cascading error messages.
static int areTypesCompatible(const char *expected, const char *actual) {
    if (!expected || !actual) return 0;

    // Suppress cascading errors if an operand already had an error
    if (strcmp(expected, "unknown") == 0 || strcmp(actual, "unknown") == 0) {
        return 1;
    }

    // Exact match
    if (strcmp(expected, actual) == 0) {
        return 1;
    }

    // Numeric conversion (e.g., int to float, float to int, char to int)
    if (isNumericType(expected) && isNumericType(actual)) {
        return 1;
    }

    // void* is compatible with any pointer in C
    if ((isPointerType(expected) && strcmp(actual, "void*") == 0) ||
        (strcmp(expected, "void*") == 0 && isPointerType(actual))) {
        return 1;
    }

    return 0;
}

// =========== SCOPE & SYMBOL TABLE MANAGEMENT ===========

// Enters a new scope by pushing a new Scope node onto the analyzer's scope stack.
void enterScope(SemanticAnalyzer *analyzer) {
    if (!analyzer) return;
    Scope *newScope = (Scope *)malloc(sizeof(Scope));
    if (!newScope) {
        fprintf(stderr, "Error: Memory allocation failed for Scope\n");
        exit(1);
    }
    newScope->symbols = NULL;
    newScope->parent = analyzer->currentScope;
    analyzer->currentScope = newScope;
}

// Exits the innermost scope, freeing all symbols that were declared inside it.
void exitScope(SemanticAnalyzer *analyzer) {
    if (!analyzer || !analyzer->currentScope) return;
    Scope *scopeToFree = analyzer->currentScope;
    analyzer->currentScope = scopeToFree->parent;

    Symbol *current = scopeToFree->symbols;
    while (current) {
        Symbol *next = current->next;
        free(current->name);
        free(current->dataType);
        if (current->paramTypes) {
            for (int i = 0; i < current->paramCount; i++) {
                free(current->paramTypes[i]);
            }
            free(current->paramTypes);
        }
        free(current);
        current = next;
    }
    free(scopeToFree);
}

// Adds a symbol to the current active scope.
Symbol *addSymbol(SemanticAnalyzer *analyzer, const char *name, const char *dataType, SymbolKind kind) {
    if (!analyzer || !analyzer->currentScope) return NULL;

    Symbol *sym = (Symbol *)malloc(sizeof(Symbol));
    if (!sym) {
        fprintf(stderr, "Error: Memory allocation failed for Symbol\n");
        exit(1);
    }
    sym->name = strdup(name ? name : "");
    sym->dataType = strdup(dataType ? dataType : "");
    sym->kind = kind;
    sym->paramTypes = NULL;
    sym->paramCount = 0;

    // Insert at head of the scope's symbol list
    sym->next = analyzer->currentScope->symbols;
    analyzer->currentScope->symbols = sym;

    return sym;
}

// Searches for an identifier by traversing the scope stack from innermost to outermost.
// This implements lexical scoping (shadowing: inner declarations hide outer ones).
Symbol *lookupSymbol(SemanticAnalyzer *analyzer, const char *name) {
    if (!analyzer || !name) return NULL;

    for (Scope *s = analyzer->currentScope; s != NULL; s = s->parent) {
        for (Symbol *sym = s->symbols; sym != NULL; sym = sym->next) {
            if (strcmp(sym->name, name) == 0) {
                return sym;
            }
        }
    }
    return NULL;
}

// Searches for an identifier ONLY in the current innermost scope.
// Used to detect illegal redeclarations within the same block.
Symbol *lookupSymbolCurrentScope(SemanticAnalyzer *analyzer, const char *name) {
    if (!analyzer || !analyzer->currentScope || !name) return NULL;

    for (Symbol *sym = analyzer->currentScope->symbols; sym != NULL; sym = sym->next) {
        if (strcmp(sym->name, name) == 0) {
            return sym;
        }
    }
    return NULL;
}

// Prints the symbols in the current scope as a formatted table.
// Useful for educational inspection of the global environment.
void printSymbolTable(SemanticAnalyzer *analyzer) {
    if (!analyzer || !analyzer->currentScope) return;

    printf("+----------------------+----------------------+--------------------------------+\n");
    printf("| Symbol Name          | Kind                 | Data Type / Signature          |\n");
    printf("+----------------------+----------------------+--------------------------------+\n");

    Symbol *sym = analyzer->currentScope->symbols;
    if (!sym) {
        printf("| (empty)                                                                      |\n");
    }

    while (sym) {
        const char *kindStr = "VARIABLE";
        if (sym->kind == SYMBOL_FUNCTION) kindStr = "FUNCTION";
        else if (sym->kind == SYMBOL_ARRAY) kindStr = "ARRAY";
        else if (sym->kind == SYMBOL_PARAMETER) kindStr = "PARAMETER";

        if (sym->kind == SYMBOL_FUNCTION) {
            char sig[128];
            if (sym->paramCount == -1) {
                snprintf(sig, sizeof(sig), "%s (...)", sym->dataType);
            } else if (sym->paramCount == 0) {
                snprintf(sig, sizeof(sig), "%s (void)", sym->dataType);
            } else {
                int offset = snprintf(sig, sizeof(sig), "%s (", sym->dataType);
                for (int i = 0; i < sym->paramCount; i++) {
                    offset += snprintf(sig + offset, sizeof(sig) - offset, "%s%s",
                                       sym->paramTypes[i], (i < sym->paramCount - 1) ? ", " : "");
                }
                snprintf(sig + offset, sizeof(sig) - offset, ")");
            }
            printf("| %-20s | %-20s | %-30s |\n", sym->name, kindStr, sig);
        } else {
            printf("| %-20s | %-20s | %-30s |\n", sym->name, kindStr, sym->dataType);
        }
        sym = sym->next;
    }
    printf("+----------------------+----------------------+--------------------------------+\n");
}

// =========== L-VALUE VALIDATION ===========

// Checks if an AST node is a valid l-value (i.e., a memory location that can be assigned to).
// In C:
//   - Identifiers (variables, parameters) are l-values, but arrays and function names are NOT.
//   - Array subscripting (arr[i]) is an l-value.
//   - Pointer dereferencing (*ptr) is an l-value.
//   - Literals and arithmetic expressions ((a + b)) are NOT l-values.
static int isLValue(SemanticAnalyzer *analyzer, ASTNode *node) {
    if (!node) return 0;

    switch (node->type) {
        case NODE_IDENTIFIER: {
            Symbol *sym = lookupSymbol(analyzer, node->data.literal.value);
            if (!sym) {
                // If not found, report undeclared later during expression evaluation
                return 1;
            }
            // In C, an entire array cannot be assigned to (e.g., arr = 5 is illegal)
            if (sym->kind == SYMBOL_ARRAY || sym->kind == SYMBOL_FUNCTION) {
                return 0;
            }
            return 1;
        }

        case NODE_ARRAY_ACCESS:
            // e.g., numbers[0] is an assignable element
            return 1;

        case NODE_UNARY_OP:
            // e.g., *ptr is an assignable location
            if (strcmp(node->data.unaryOp.op, "*") == 0) {
                return 1;
            }
            return 0;

        default:
            return 0;
    }
}

// =========== EXPRESSION ANALYSIS ===========

// Recursively analyzes an expression AST node and infers its resulting data type.
// Returns a dynamically allocated string with the type name (e.g. "int", "float", "char*"),
// or "unknown" if an error occurred. The caller is responsible for calling free() on the result.
static char *analyzeExpression(SemanticAnalyzer *analyzer, ASTNode *node) {
    if (!node) return strdup("unknown");

    switch (node->type) {
        case NODE_INTEGER_LITERAL:
            return strdup("int");

        case NODE_FLOAT_LITERAL:
            return strdup("float");

        case NODE_STRING_LITERAL:
            // String literals in C have type char*
            return strdup("char*");

        case NODE_IDENTIFIER: {
            const char *name = node->data.literal.value;
            Symbol *sym = lookupSymbol(analyzer, name);
            if (!sym) {
                semanticError(analyzer, "Use of undeclared identifier '%s'", name);
                return strdup("unknown");
            }

            // In C expressions, an array identifier decays to a pointer to its element type:
            // e.g., int arr[10] used in an expression evaluates to type 'int*'
            if (sym->kind == SYMBOL_ARRAY) {
                return addPointer(sym->dataType);
            }

            return strdup(sym->dataType);
        }

        case NODE_BINARY_OP: {
            char *leftType = analyzeExpression(analyzer, node->data.binaryOp.left);
            char *rightType = analyzeExpression(analyzer, node->data.binaryOp.right);
            const char *op = node->data.binaryOp.op;

            // Suppress cascading errors if any operand is invalid
            if (strcmp(leftType, "unknown") == 0 || strcmp(rightType, "unknown") == 0) {
                free(leftType);
                free(rightType);
                return strdup("unknown");
            }

            // Arithmetic: +, -, *, /
            if (strcmp(op, "+") == 0 || strcmp(op, "-") == 0 ||
                strcmp(op, "*") == 0 || strcmp(op, "/") == 0) {

                // Both operands are numeric
                if (isNumericType(leftType) && isNumericType(rightType)) {
                    char *result;
                    if (strcmp(leftType, "float") == 0 || strcmp(rightType, "float") == 0 ||
                        strcmp(leftType, "double") == 0 || strcmp(rightType, "double") == 0) {
                        result = strdup("float");
                    } else {
                        result = strdup("int");
                    }
                    free(leftType);
                    free(rightType);
                    return result;
                }

                // Pointer arithmetic: pointer + int -> pointer, pointer - int -> pointer
                if (strcmp(op, "+") == 0) {
                    if (isPointerType(leftType) && isIntegerType(rightType)) {
                        free(rightType);
                        return leftType;
                    }
                    if (isIntegerType(leftType) && isPointerType(rightType)) {
                        free(leftType);
                        return rightType;
                    }
                } else if (strcmp(op, "-") == 0) {
                    if (isPointerType(leftType) && isIntegerType(rightType)) {
                        free(rightType);
                        return leftType;
                    }
                    // pointer - pointer -> int (ptrdiff_t)
                    if (isPointerType(leftType) && isPointerType(rightType) && strcmp(leftType, rightType) == 0) {
                        free(leftType);
                        free(rightType);
                        return strdup("int");
                    }
                }

                semanticError(analyzer, "Invalid operands to binary operator '%s' ('%s' and '%s')",
                              op, leftType, rightType);
                free(leftType);
                free(rightType);
                return strdup("unknown");
            }

            // Modulo: % (requires integers)
            if (strcmp(op, "%") == 0) {
                if (!isIntegerType(leftType) || !isIntegerType(rightType)) {
                    semanticError(analyzer, "Operands to '%%' must be integer types (got '%s' and '%s')",
                                  leftType, rightType);
                }
                free(leftType);
                free(rightType);
                return strdup("int");
            }

            // Comparison: <, <=, >, >=
            if (strcmp(op, "<") == 0 || strcmp(op, "<=") == 0 ||
                strcmp(op, ">") == 0 || strcmp(op, ">=") == 0) {
                if (!((isNumericType(leftType) && isNumericType(rightType)) ||
                      (isPointerType(leftType) && isPointerType(rightType)))) {
                    semanticError(analyzer, "Comparison '%s' between incompatible types ('%s' and '%s')",
                                  op, leftType, rightType);
                }
                free(leftType);
                free(rightType);
                return strdup("int");
            }

            // Equality: ==, !=
            if (strcmp(op, "==") == 0 || strcmp(op, "!=") == 0) {
                if (!areTypesCompatible(leftType, rightType) &&
                    !(isPointerType(leftType) && isPointerType(rightType))) {
                    semanticError(analyzer, "Comparison '%s' between incompatible types ('%s' and '%s')",
                                  op, leftType, rightType);
                }
                free(leftType);
                free(rightType);
                return strdup("int");
            }

            // Logical: &&, ||
            if (strcmp(op, "&&") == 0 || strcmp(op, "||") == 0) {
                if (strcmp(leftType, "void") == 0 || strcmp(rightType, "void") == 0) {
                    semanticError(analyzer, "Operands to logical '%s' cannot be void", op);
                }
                free(leftType);
                free(rightType);
                return strdup("int");
            }

            free(leftType);
            free(rightType);
            return strdup("int");
        }

        case NODE_UNARY_OP: {
            const char *op = node->data.unaryOp.op;
            ASTNode *operand = node->data.unaryOp.operand;

            // Address-of: &
            if (strcmp(op, "&") == 0) {
                if (!isLValue(analyzer, operand)) {
                    semanticError(analyzer, "Cannot take address of non-lvalue");
                }
                char *opType = analyzeExpression(analyzer, operand);
                if (strcmp(opType, "unknown") == 0) return opType;
                char *ptrType = addPointer(opType);
                free(opType);
                return ptrType;
            }

            // Dereference: *
            if (strcmp(op, "*") == 0) {
                char *opType = analyzeExpression(analyzer, operand);
                if (strcmp(opType, "unknown") == 0) return opType;
                if (!isPointerType(opType)) {
                    semanticError(analyzer, "Cannot dereference non-pointer type '%s'", opType);
                    free(opType);
                    return strdup("unknown");
                }
                char *baseType = removePointer(opType);
                free(opType);
                return baseType;
            }

            // Increment / Decrement: ++, --
            if (strcmp(op, "++") == 0 || strcmp(op, "--") == 0) {
                if (!isLValue(analyzer, operand)) {
                    semanticError(analyzer, "Operand of '%s' must be an lvalue", op);
                }
                char *opType = analyzeExpression(analyzer, operand);
                if (strcmp(opType, "unknown") != 0 &&
                    !isNumericType(opType) && !isPointerType(opType)) {
                    semanticError(analyzer, "Operand of '%s' must be numeric or pointer (got '%s')", op, opType);
                }
                return opType;
            }

            // Unary + and -
            if (strcmp(op, "+") == 0 || strcmp(op, "-") == 0) {
                char *opType = analyzeExpression(analyzer, operand);
                if (strcmp(opType, "unknown") != 0 && !isNumericType(opType)) {
                    semanticError(analyzer, "Operand of unary '%s' must be numeric (got '%s')", op, opType);
                }
                return opType;
            }

            // Logical NOT: !
            if (strcmp(op, "!") == 0) {
                char *opType = analyzeExpression(analyzer, operand);
                if (strcmp(opType, "void") == 0) {
                    semanticError(analyzer, "Operand of '!' cannot be void");
                }
                free(opType);
                return strdup("int");
            }

            // Bitwise NOT: ~
            if (strcmp(op, "~") == 0) {
                char *opType = analyzeExpression(analyzer, operand);
                if (strcmp(opType, "unknown") != 0 && !isIntegerType(opType)) {
                    semanticError(analyzer, "Operand of '~' must be an integer (got '%s')", opType);
                }
                free(opType);
                return strdup("int");
            }

            return analyzeExpression(analyzer, operand);
        }

        case NODE_ASSIGNMENT: {
            const char *op = node->data.binaryOp.op;
            ASTNode *left = node->data.binaryOp.left;
            ASTNode *right = node->data.binaryOp.right;

            // 1. Verify left-hand side is an assignable l-value
            if (!isLValue(analyzer, left)) {
                semanticError(analyzer, "Left hand side of assignment '%s' must be an lvalue", op);
            }

            char *leftType = analyzeExpression(analyzer, left);
            char *rightType = analyzeExpression(analyzer, right);

            // 2. Verify type compatibility between destination and source
            if (strcmp(leftType, "unknown") != 0 && strcmp(rightType, "unknown") != 0) {
                if (!areTypesCompatible(leftType, rightType)) {
                    semanticError(analyzer, "Incompatible types in assignment: cannot assign '%s' to '%s'",
                                  rightType, leftType);
                }
            }

            free(rightType);
            return leftType;
        }

        case NODE_FUNC_CALL: {
            const char *name = node->data.funCall.name;
            Symbol *sym = lookupSymbol(analyzer, name);

            if (!sym) {
                semanticError(analyzer, "Call to undeclared function '%s'", name);
                // Still analyze arguments to check for errors in their expressions
                for (int i = 0; i < node->data.funCall.argCount; i++) {
                    char *argType = analyzeExpression(analyzer, node->data.funCall.args[i]);
                    free(argType);
                }
                return strdup("unknown");
            }

            if (sym->kind != SYMBOL_FUNCTION) {
                semanticError(analyzer, "Called object '%s' is not a function", name);
                for (int i = 0; i < node->data.funCall.argCount; i++) {
                    char *argType = analyzeExpression(analyzer, node->data.funCall.args[i]);
                    free(argType);
                }
                return strdup("unknown");
            }

            // Check argument count (if paramCount >= 0; -1 indicates variadic functions like printf)
            if (sym->paramCount >= 0) {
                if (node->data.funCall.argCount != sym->paramCount) {
                    semanticError(analyzer, "Function '%s' expects %d argument(s), but %d were provided",
                                  name, sym->paramCount, node->data.funCall.argCount);
                }
            }

            // Check argument types against declared parameter types
            int checkCount = (sym->paramCount >= 0 && node->data.funCall.argCount > sym->paramCount)
                             ? sym->paramCount
                             : node->data.funCall.argCount;

            for (int i = 0; i < node->data.funCall.argCount; i++) {
                char *argType = analyzeExpression(analyzer, node->data.funCall.args[i]);
                if (sym->paramCount >= 0 && i < checkCount) {
                    if (!areTypesCompatible(sym->paramTypes[i], argType)) {
                        semanticError(analyzer, "Incompatible type for argument %d of '%s': expected '%s', got '%s'",
                                      i + 1, name, sym->paramTypes[i], argType);
                    }
                }
                free(argType);
            }

            return strdup(sym->dataType);
        }

        case NODE_ARRAY_ACCESS: {
            char *arrType = analyzeExpression(analyzer, node->data.arrayAccess.array);
            char *idxType = analyzeExpression(analyzer, node->data.arrayAccess.index);

            // 1. Array index must be an integer
            if (strcmp(idxType, "unknown") != 0 && !isIntegerType(idxType)) {
                semanticError(analyzer, "Array subscript is not an integer (got '%s')", idxType);
            }

            if (strcmp(arrType, "unknown") == 0) {
                free(arrType);
                free(idxType);
                return strdup("unknown");
            }

            // 2. Base expression must be an array or pointer
            if (!isPointerType(arrType)) {
                semanticError(analyzer, "Subscripted value is not an array or pointer (got '%s')", arrType);
                free(arrType);
                free(idxType);
                return strdup("unknown");
            }

            // Dereference: e.g. int* -> int
            char *elemType = removePointer(arrType);
            free(arrType);
            free(idxType);
            return elemType;
        }

        default:
            return strdup("unknown");
    }
}

// =========== STATEMENT ANALYSIS ===========

// Recursively walks statement AST nodes, managing scopes, declarations, and control flow.
static void analyzeStatement(SemanticAnalyzer *analyzer, ASTNode *node) {
    if (!node) return;

    switch (node->type) {
        case NODE_PROGRAM: {
            for (int i = 0; i < node->data.program.count; i++) {
                analyzeStatement(analyzer, node->data.program.declarations[i]);
            }
            break;
        }

        case NODE_COMPOUND_STMT: {
            // A compound statement { ... } introduces a new nested block scope
            enterScope(analyzer);
            for (int i = 0; i < node->data.compoundStmt.count; i++) {
                analyzeStatement(analyzer, node->data.compoundStmt.statements[i]);
            }
            exitScope(analyzer);
            break;
        }

        case NODE_VAR_DECL: {
            const char *name = node->data.varDecl.name;
            const char *dataType = node->data.varDecl.dataType;

            // In C, variables cannot have type 'void' (though 'void*' is valid)
            if (strcmp(dataType, "void") == 0) {
                semanticError(analyzer, "Variable '%s' declared with void type", name);
            }

            // Check if already declared in the current block
            if (lookupSymbolCurrentScope(analyzer, name)) {
                semanticError(analyzer, "Redeclaration of '%s' in the same scope", name);
            }

            // If initialized, check compatibility between variable type and init expression
            if (node->data.varDecl.init) {
                char *initType = analyzeExpression(analyzer, node->data.varDecl.init);
                if (!areTypesCompatible(dataType, initType)) {
                    semanticError(analyzer, "Incompatible initialization of '%s': expected '%s', got '%s'",
                                  name, dataType, initType);
                }
                free(initType);
            }

            addSymbol(analyzer, name, dataType, SYMBOL_VARIABLE);
            break;
        }

        case NODE_ARRAY_DECL: {
            const char *name = node->data.arrayDecl.name;
            const char *dataType = node->data.arrayDecl.dataType;

            if (strcmp(dataType, "void") == 0) {
                semanticError(analyzer, "Array '%s' cannot have void element type", name);
            }

            if (lookupSymbolCurrentScope(analyzer, name)) {
                semanticError(analyzer, "Redeclaration of '%s' in the same scope", name);
            }

            // Check array size expression (must be an integer)
            if (node->data.arrayDecl.size) {
                char *sizeType = analyzeExpression(analyzer, node->data.arrayDecl.size);
                if (strcmp(sizeType, "unknown") != 0 && !isIntegerType(sizeType)) {
                    semanticError(analyzer, "Array size must be an integer (got '%s')", sizeType);
                }
                free(sizeType);
            }

            if (node->data.arrayDecl.init) {
                char *initType = analyzeExpression(analyzer, node->data.arrayDecl.init);
                free(initType);
            }

            addSymbol(analyzer, name, dataType, SYMBOL_ARRAY);
            break;
        }

        case NODE_FUNC_DECL: {
            const char *name = node->data.funDecl.name;
            const char *returnType = node->data.funDecl.returnType;

            // Standard C does not support nested function definitions
            if (analyzer->currentScope->parent != NULL) {
                semanticError(analyzer, "Nested function definitions are not supported ('%s')", name);
            }

            if (lookupSymbolCurrentScope(analyzer, name)) {
                semanticError(analyzer, "Redefinition of function '%s'", name);
            }

            // Register function in the global symbol table
            Symbol *funcSym = addSymbol(analyzer, name, returnType, SYMBOL_FUNCTION);
            funcSym->paramCount = node->data.funDecl.paramCount;
            if (funcSym->paramCount > 0) {
                funcSym->paramTypes = (char **)malloc(sizeof(char *) * funcSym->paramCount);
                for (int i = 0; i < funcSym->paramCount; i++) {
                    funcSym->paramTypes[i] = strdup(node->data.funDecl.params[i]->data.varDecl.dataType);
                }
            }

            // If function has an implementation body, analyze its parameters and statements
            if (node->data.funDecl.body) {
                char *prevReturnType = analyzer->currentFunctionReturnType;
                analyzer->currentFunctionReturnType = (char *)returnType;

                // Function scope: holds parameters and top-level local variables
                enterScope(analyzer);

                for (int i = 0; i < node->data.funDecl.paramCount; i++) {
                    const char *pName = node->data.funDecl.params[i]->data.varDecl.name;
                    const char *pType = node->data.funDecl.params[i]->data.varDecl.dataType;

                    if (strcmp(pType, "void") == 0) {
                        semanticError(analyzer, "Parameter '%s' cannot have void type", pName);
                    }
                    if (strlen(pName) > 0) {
                        if (lookupSymbolCurrentScope(analyzer, pName)) {
                            semanticError(analyzer, "Duplicate parameter name '%s' in function '%s'", pName, name);
                        } else {
                            addSymbol(analyzer, pName, pType, SYMBOL_PARAMETER);
                        }
                    }
                }

                // Analyze the body: if the body is a compound statement, analyze its statements directly
                // in the function scope so top-level variables share scope with parameters
                if (node->data.funDecl.body->type == NODE_COMPOUND_STMT) {
                    for (int i = 0; i < node->data.funDecl.body->data.compoundStmt.count; i++) {
                        analyzeStatement(analyzer, node->data.funDecl.body->data.compoundStmt.statements[i]);
                    }
                } else {
                    analyzeStatement(analyzer, node->data.funDecl.body);
                }

                exitScope(analyzer);
                analyzer->currentFunctionReturnType = prevReturnType;
            }
            break;
        }

        case NODE_IF_STMT: {
            char *condType = analyzeExpression(analyzer, node->data.ifStmt.condition);
            free(condType);
            analyzeStatement(analyzer, node->data.ifStmt.thenBranch);
            if (node->data.ifStmt.elseBranch) {
                analyzeStatement(analyzer, node->data.ifStmt.elseBranch);
            }
            break;
        }

        case NODE_WHILE_STMT: {
            char *condType = analyzeExpression(analyzer, node->data.loop.condition);
            free(condType);

            // Increment loop depth to allow break and continue
            analyzer->loopDepth++;
            analyzeStatement(analyzer, node->data.loop.body);
            analyzer->loopDepth--;
            break;
        }

        case NODE_FOR_STMT: {
            // A for-loop introduces a new scope (for C99-style 'for (int i = 0; ...)')
            enterScope(analyzer);

            if (node->data.loop.init) {
                analyzeStatement(analyzer, node->data.loop.init);
            }
            if (node->data.loop.condition) {
                char *condType = analyzeExpression(analyzer, node->data.loop.condition);
                free(condType);
            }
            if (node->data.loop.increment) {
                char *incType = analyzeExpression(analyzer, node->data.loop.increment);
                free(incType);
            }

            analyzer->loopDepth++;
            analyzeStatement(analyzer, node->data.loop.body);
            analyzer->loopDepth--;

            exitScope(analyzer);
            break;
        }

        case NODE_RETURN_STMT: {
            if (!analyzer->currentFunctionReturnType) {
                semanticError(analyzer, "'return' statement outside of a function");
                if (node->data.returnStmt.expr) {
                    char *t = analyzeExpression(analyzer, node->data.returnStmt.expr);
                    free(t);
                }
                break;
            }

            // Function returning void
            if (strcmp(analyzer->currentFunctionReturnType, "void") == 0) {
                if (node->data.returnStmt.expr) {
                    char *retType = analyzeExpression(analyzer, node->data.returnStmt.expr);
                    semanticError(analyzer, "Void function cannot return a value (got '%s')", retType);
                    free(retType);
                }
            } else {
                // Function returning a non-void value
                if (!node->data.returnStmt.expr) {
                    semanticError(analyzer, "Non-void function must return a value of type '%s'",
                                  analyzer->currentFunctionReturnType);
                } else {
                    char *retType = analyzeExpression(analyzer, node->data.returnStmt.expr);
                    if (!areTypesCompatible(analyzer->currentFunctionReturnType, retType)) {
                        semanticError(analyzer, "Incompatible return type: expected '%s', got '%s'",
                                      analyzer->currentFunctionReturnType, retType);
                    }
                    free(retType);
                }
            }
            break;
        }

        case NODE_BREAK_STMT: {
            if (analyzer->loopDepth <= 0) {
                semanticError(analyzer, "'break' statement not within a loop");
            }
            break;
        }

        case NODE_CONTINUE_STMT: {
            if (analyzer->loopDepth <= 0) {
                semanticError(analyzer, "'continue' statement not within a loop");
            }
            break;
        }

        case NODE_EXPRESSION_STMT: {
            if (node->data.exprStmt.expr) {
                char *t = analyzeExpression(analyzer, node->data.exprStmt.expr);
                free(t);
            }
            break;
        }

        case NODE_PARAM_DECL:
            // Handled inside NODE_FUNC_DECL
            break;

        default:
            // If an expression node is somehow passed directly as a statement, analyze it
            {
                char *t = analyzeExpression(analyzer, node);
                free(t);
            }
            break;
    }
}

// =========== ANALYZER LIFECYCLE ===========

SemanticAnalyzer *createSemanticAnalyzer(void) {
    SemanticAnalyzer *analyzer = (SemanticAnalyzer *)malloc(sizeof(SemanticAnalyzer));
    if (!analyzer) {
        fprintf(stderr, "Error: Memory allocation failed for SemanticAnalyzer\n");
        exit(1);
    }
    analyzer->currentScope = NULL;
    analyzer->currentFunctionReturnType = NULL;
    analyzer->loopDepth = 0;
    analyzer->errorCount = 0;

    // Create the global scope
    enterScope(analyzer);

    // Pre-register standard built-in functions (e.g. printf with variadic arguments)
    Symbol *printfSym = addSymbol(analyzer, "printf", "int", SYMBOL_FUNCTION);
    printfSym->paramCount = -1; // -1 denotes variadic parameters

    return analyzer;
}

void freeSemanticAnalyzer(SemanticAnalyzer *analyzer) {
    if (!analyzer) return;
    while (analyzer->currentScope) {
        exitScope(analyzer);
    }
    free(analyzer);
}

// =========== ENTRY POINT ===========

int analyzeProgram(SemanticAnalyzer *analyzer, ASTNode *program) {
    if (!analyzer || !program) return -1;

    analyzeStatement(analyzer, program);

    // Optional check: verify if 'main' exists
    Symbol *mainSym = lookupSymbol(analyzer, "main");
    if (!mainSym) {
        printf("Semantic Warning: Program does not define a 'main' function.\n");
    } else if (mainSym->kind != SYMBOL_FUNCTION) {
        semanticError(analyzer, "'main' must be a function");
    }

    return analyzer->errorCount;
}
