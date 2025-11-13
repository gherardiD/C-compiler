#ifndef PARSER.H
#define PARSER.H

#include "lexer.h"

// =========== AST NODES TYPES ===========

typedef enum {
    // Literals
    NODE_INTEGER_LITERAL,
    NODE_FLOAT_LITERAL,
    NODE_STRING_LITERAL,
    NODE_IDENTIFIER,

    // Operations and assignments
    NODE_BINARY_OP,
    NODE_UNARY_OP,   // ++, --, etc.
    NODE_ASSIGNMENT,    // =, +=, -=, etc.

    // Control flow
    NODE_EXPRESSION_STMT,
    NODE_COMPOUND_STMT,
    NODE_IF_STMT,
    NODE_WHILE_STMT,
    NODE_FOR_STMT,
    NODE_RETURN_STMT,
    NODE_BREAK_STMT,
    NODE_CONTINUE_STMT,

    // declarations
    NODE_FUNC_DECL,
    NODE_VAR_DECL,
    NODE_PARAM_DECL,
    NODE_ARRAY_DECL,

    // access
    NODE_FUNC_CALL,
    NODE_ARRAY_ACCESS,

    // Program root
    NODE_PROGRAM

} ASTNodeType;



// =========== AST NODES STRUCTURE ===========

typedef struct ASTNode {
    ASTNodeType type;

    // using an union is more memory efficient each node instead of having all fields
    // it has only the fields it needs
    union
    {
        // literals and Identifiers
        struct {
            char* value;
        } literal;

        // binary operations
        struct {
            char *op;
            struct ASTNode *left;
            struct ASTNode *right;
        } binaryOp;

        // unary operations
        struct {
            char *op;
            struct ASTNode *operand;
        } unaryOp;

        // variable declaration
        struct {
            char *dataType;
            char *name;
            struct ASTNode *init;
        } varDecl;

        // function declaration
        struct {
            char *returnType;
            char *name;
            struct ASTNode **params;
            int paramCount;
            struct ASTNode *body;
        } funDecl;

        // if statement
        struct {
            struct ASTNode *condition;
            struct ASTNode *thenBranch;
            struct ASTNode *elseBranch;
        } ifStmt;

        // while / for loops
        struct {
            struct ASTNode *condition;
            struct ASTNode *body;
            struct ASTNode *init;
            struct ASTNode *increment;
        } loop;

        // compound statement
        struct {
            struct ASTNode **statements;
            int count;
            int capacity;
        } compoundStmt;

        // function call
        struct {
            char *name;
            struct ASTNode **args;
            int argCount;
        } funCall;

        // return statement
        struct {
            struct ASTNode *expr;
        } returnStmt;

    } data;
} ASTNode;


typedef struct {
    Token *current_token;
    int position;
    int token_count;
    Token *tokens;
} Parser;

// =========== PARSER FUNCTIONS ===========

Parser *createParser(Token *tokens, int token_count);
void freeParser(Parser *parser);

// main parsing function
ASTNode *parseProgram(Parser *parser);

// Base AST node creation function
ASTNode *createASTNode(ASTNodeType type);

// helpers to create specific AST nodes
ASTNode *createASTBinaryOp(const char *op, ASTNode *left, ASTNode *right);
ASTNode *createASTBinaryOp(const char *op, ASTNode *operand);
ASTNode *createASTLiteral(ASTNodeType type, const char *value);
ASTNode *createASTVarDecl(const char *dataType, const char *name, ASTNode *init);

void freeAST(ASTNode *node);
void addChild(ASTNode *parent, ASTNode *child);

// token navigation functions
void advance(Parser *parser);
Token *peek(Parser *parser, int offset);
int match(Parser *parser, TokenType type);
int matchValue(Parser *parser, const char *value);
int expect(Parser *parser, TokenType type);
int expextValue(Parser *parser, const char *value);


ASTNode* parseProgram(Parser *parser);           // Entry point for parsing
ASTNode* parseDeclaration(Parser *parser);       // int x; or int foo() {...}
ASTNode* parseFunctionDecl(Parser *parser, char *returnType, char *name);
ASTNode* parseStatement(Parser *parser);         // Statements (if, while, return, etc.)
ASTNode* parseCompoundStmt(Parser *parser);
ASTNode* parseIfStmt(Parser *parser);
ASTNode* parseWhileStmt(Parser *parser);
ASTNode* parseForStmt(Parser *parser);
ASTNode* parseReturnStmt(Parser *parser);
ASTNode* parseExpressionStmt(Parser *parser);


ASTNode* parseExpression(Parser *parser);        // Entry point for expressions
ASTNode* parseAssignment(Parser *parser);        // = += -= *= /=
ASTNode* parseLogicalOr(Parser *parser);         // ||
ASTNode* parseLogicalAnd(Parser *parser);        // &&
ASTNode* parseEquality(Parser *parser);          // == !=
ASTNode* parseRelational(Parser *parser);        // < > <= >=
ASTNode* parseAdditive(Parser *parser);          // + -
ASTNode* parseMultiplicative(Parser *parser);    // * / %
ASTNode* parseUnary(Parser *parser);             // - ! & * ++ --
ASTNode* parsePostfix(Parser *parser);           // arr[i] func() ++ --
ASTNode* parsePrimary(Parser *parser);           // identifiers, literals, (expr)


void printAST(ASTNode *node, int level);
int isDataType(const char *str);

#endif
