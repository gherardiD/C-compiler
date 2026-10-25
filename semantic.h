#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "parser.h"

// =======================================================================
// SEMANTIC ANALYSIS MODULE
// =======================================================================
// Semantic analysis is the phase of the compiler that checks whether the
// program makes logical sense according to the language rules, beyond just
// grammar. It answers questions like:
//  - Has a variable been declared before being used?
//  - Are data types compatible in operations and assignments?
//  - Are function calls providing the right number and type of arguments?
//  - Are 'break' and 'continue' only used inside loops?
//  - Does a function return a value matching its declared return type?
// =======================================================================

// =========== SYMBOL TABLE TYPES ===========

// What kind of entity a symbol represents in the program
typedef enum {
    SYMBOL_VARIABLE,   // A normal variable (e.g., int x;)
    SYMBOL_ARRAY,      // An array (e.g., int arr[10];)
    SYMBOL_FUNCTION,   // A function (e.g., int add(int a, int b);)
    SYMBOL_PARAMETER   // A function parameter (e.g., a in add(int a))
} SymbolKind;

// One entry in the symbol table.
// Each symbol stores its identifier name, data type, kind, and if it is
// a function, information about its parameters for call-site verification.
typedef struct Symbol {
    char *name;              // Identifier name (e.g., "counter", "add")
    char *dataType;          // Type as string (e.g., "int", "float", "char*")
    SymbolKind kind;         // Variable, array, function, or parameter
    char **paramTypes;       // Parameter types for functions (e.g., ["int", "int"])
    int paramCount;          // Number of parameters (-1 for variadic like printf)
    struct Symbol *next;     // Linked list pointer to the next symbol in this scope
} Symbol;

// A Scope represents a block of code enclosed in { ... }, a function body,
// or the global program scope.
// Scopes form a stack (parent pointers): symbol resolution searches from the
// current innermost scope up towards the global scope.
typedef struct Scope {
    Symbol *symbols;         // Head of the linked list of symbols declared in this scope
    struct Scope *parent;    // Enclosing (outer) scope, or NULL for global scope
} Scope;

// =========== SEMANTIC ANALYZER STRUCTURE ===========

// The analyzer maintains state during the traversal of the AST.
typedef struct {
    Scope *currentScope;              // Pointer to the current active scope
    char *currentFunctionReturnType;  // Return type of the function currently being analyzed
    int loopDepth;                    // > 0 means we are inside a while/for loop
    int errorCount;                   // Total number of semantic errors found
} SemanticAnalyzer;

// =========== LIFECYCLE FUNCTIONS ===========

// Allocates and initializes a new SemanticAnalyzer with a global scope.
SemanticAnalyzer *createSemanticAnalyzer(void);

// Frees all resources allocated by the analyzer, including any remaining scopes and symbols.
void freeSemanticAnalyzer(SemanticAnalyzer *analyzer);

// =========== SCOPE & SYMBOL TABLE MANAGEMENT ===========

// Enters a new child scope (e.g., when entering a block { ... } or function).
void enterScope(SemanticAnalyzer *analyzer);

// Exits the current scope and frees its local symbols, returning to the parent scope.
void exitScope(SemanticAnalyzer *analyzer);

// Adds a symbol to the current scope.
Symbol *addSymbol(SemanticAnalyzer *analyzer, const char *name, const char *dataType, SymbolKind kind);

// Searches for a symbol by name from the current scope outward to the global scope.
Symbol *lookupSymbol(SemanticAnalyzer *analyzer, const char *name);

// Searches for a symbol by name ONLY in the current scope (used to detect re-declarations).
Symbol *lookupSymbolCurrentScope(SemanticAnalyzer *analyzer, const char *name);

// Prints the symbols in the current scope (used to display the global symbol table).
void printSymbolTable(SemanticAnalyzer *analyzer);

// =========== ANALYSIS ENTRY POINT ===========

// Walks the AST, performs all scope, declaration, and type checks,
// and returns the total number of semantic errors encountered.
int analyzeProgram(SemanticAnalyzer *analyzer, ASTNode *program);

#endif
