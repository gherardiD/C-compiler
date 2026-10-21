#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Helper to check if parser has reached the end of tokens
static int isAtEnd(Parser *parser) {
    if (!parser || !parser->current_token) return 1;
    if (parser->current_token->type == TOKEN_INVALID &&
        parser->current_token->value &&
        strcmp(parser->current_token->value, "EOF") == 0) {
        return 1;
    }
    if (parser->position >= parser->token_count) return 1;
    return 0;
}

// =========== PARSER LIFECYCLE ===========

Parser *createParser(Token *tokens, int token_count) {
    Parser *parser = (Parser *)malloc(sizeof(Parser));
    if (!parser) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        exit(1);
    }

    if (token_count <= 0 && tokens != NULL) {
        token_count = 0;
        while (tokens[token_count].value != NULL) {
            token_count++;
            if (tokens[token_count - 1].type == TOKEN_INVALID &&
                strcmp(tokens[token_count - 1].value, "EOF") == 0) {
                break;
            }
        }
    }

    parser->tokens = tokens;
    parser->token_count = token_count;
    parser->position = 0;
    parser->current_token = (tokens && token_count > 0) ? &tokens[0] : NULL;
    return parser;
}

void freeParser(Parser *parser) {
    if (!parser) return;
    free(parser);
}

// =========== AST NODE CREATION & FREEING ===========

ASTNode *createASTNode(ASTNodeType type) {
    ASTNode *node = (ASTNode *)malloc(sizeof(ASTNode));
    if (!node) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        exit(1);
    }

    node->type = type;
    memset(&node->data, 0, sizeof(node->data));
    return node;
}

ASTNode *createASTBinaryOp(const char *op, ASTNode *left, ASTNode *right) {
    ASTNode *node = createASTNode(NODE_BINARY_OP);
    node->data.binaryOp.op = strdup(op);
    node->data.binaryOp.left = left;
    node->data.binaryOp.right = right;
    return node;
}

ASTNode *createASTUnaryOp(const char *op, ASTNode *operand) {
    ASTNode *node = createASTNode(NODE_UNARY_OP);
    node->data.unaryOp.op = strdup(op);
    node->data.unaryOp.operand = operand;
    return node;
}

ASTNode *createASTLiteral(ASTNodeType type, const char *value) {
    ASTNode *node = createASTNode(type);
    node->data.literal.value = strdup(value ? value : "");
    return node;
}

ASTNode *createASTVarDecl(const char *dataType, const char *name, ASTNode *init) {
    ASTNode *node = createASTNode(NODE_VAR_DECL);
    node->data.varDecl.dataType = strdup(dataType ? dataType : "");
    node->data.varDecl.name = strdup(name ? name : "");
    node->data.varDecl.init = init;
    return node;
}

void freeAST(ASTNode *node) {
    if (!node) return;

    switch (node->type) {
        case NODE_INTEGER_LITERAL:
        case NODE_FLOAT_LITERAL:
        case NODE_STRING_LITERAL:
        case NODE_IDENTIFIER:
            free(node->data.literal.value);
            break;

        case NODE_BINARY_OP:
        case NODE_ASSIGNMENT:
            free(node->data.binaryOp.op);
            freeAST(node->data.binaryOp.left);
            freeAST(node->data.binaryOp.right);
            break;

        case NODE_UNARY_OP:
            free(node->data.unaryOp.op);
            freeAST(node->data.unaryOp.operand);
            break;

        case NODE_VAR_DECL:
        case NODE_PARAM_DECL:
            free(node->data.varDecl.dataType);
            free(node->data.varDecl.name);
            freeAST(node->data.varDecl.init);
            break;

        case NODE_ARRAY_DECL:
            free(node->data.arrayDecl.dataType);
            free(node->data.arrayDecl.name);
            freeAST(node->data.arrayDecl.size);
            freeAST(node->data.arrayDecl.init);
            break;

        case NODE_ARRAY_ACCESS:
            freeAST(node->data.arrayAccess.array);
            freeAST(node->data.arrayAccess.index);
            break;

        case NODE_IF_STMT:
            freeAST(node->data.ifStmt.condition);
            freeAST(node->data.ifStmt.thenBranch);
            freeAST(node->data.ifStmt.elseBranch);
            break;

        case NODE_WHILE_STMT:
        case NODE_FOR_STMT:
            freeAST(node->data.loop.condition);
            freeAST(node->data.loop.body);
            freeAST(node->data.loop.init);
            freeAST(node->data.loop.increment);
            break;

        case NODE_COMPOUND_STMT:
            for (int i = 0; i < node->data.compoundStmt.count; i++) {
                freeAST(node->data.compoundStmt.statements[i]);
            }
            free(node->data.compoundStmt.statements);
            break;

        case NODE_FUNC_CALL:
            free(node->data.funCall.name);
            for (int i = 0; i < node->data.funCall.argCount; i++) {
                freeAST(node->data.funCall.args[i]);
            }
            free(node->data.funCall.args);
            break;

        case NODE_FUNC_DECL:
            free(node->data.funDecl.returnType);
            free(node->data.funDecl.name);
            for (int i = 0; i < node->data.funDecl.paramCount; i++) {
                freeAST(node->data.funDecl.params[i]);
            }
            free(node->data.funDecl.params);
            freeAST(node->data.funDecl.body);
            break;

        case NODE_RETURN_STMT:
            freeAST(node->data.returnStmt.expr);
            break;

        case NODE_EXPRESSION_STMT:
            freeAST(node->data.exprStmt.expr);
            break;

        case NODE_PROGRAM:
            for (int i = 0; i < node->data.program.count; i++) {
                freeAST(node->data.program.declarations[i]);
            }
            free(node->data.program.declarations);
            break;

        case NODE_BREAK_STMT:
        case NODE_CONTINUE_STMT:
        default:
            break;
    }

    free(node);
}

// =========== TOKEN NAVIGATION FUNCTIONS ===========

void advance(Parser *parser) {
    if (!parser) return;
    if (parser->position < parser->token_count - 1) {
        parser->position++;
        parser->current_token = &parser->tokens[parser->position];
    }
}

Token *peek(Parser *parser, int offset) {
    if (!parser) return NULL;
    int idx = parser->position + offset;
    if (idx < 0 || idx >= parser->token_count) {
        return NULL;
    }
    return &parser->tokens[idx];
}

int match(Parser *parser, TokenType type) {
    if (!parser || !parser->current_token) return 0;
    return parser->current_token->type == type;
}

int matchValue(Parser *parser, const char *value) {
    if (!parser || !parser->current_token || !parser->current_token->value || !value) return 0;
    return strcmp(parser->current_token->value, value) == 0;
}

int expect(Parser *parser, TokenType type) {
    if (!parser || !parser->current_token) return 0;
    if (match(parser, type)) {
        advance(parser);
        return 1;
    }
    fprintf(stderr, "Syntax Error: Expected token type %d, got '%s'\n",
            type, parser->current_token->value ? parser->current_token->value : "(null)");
    return 0;
}

int expextValue(Parser *parser, const char *value) {
    if (!parser || !parser->current_token) return 0;
    if (matchValue(parser, value)) {
        advance(parser);
        return 1;
    }
    fprintf(stderr, "Syntax Error: Expected '%s', got '%s'\n",
            value ? value : "(null)",
            parser->current_token->value ? parser->current_token->value : "(null)");
    return 0;
}

int expectValue(Parser *parser, const char *value) {
    return expextValue(parser, value);
}

int isDataType(const char *str) {
    if (!str) return 0;
    const char *types[] = {
        "int", "float", "char", "double", "void",
        "long", "short", "signed", "unsigned"
    };
    int len = sizeof(types) / sizeof(types[0]);
    for (int i = 0; i < len; i++) {
        if (strcmp(str, types[i]) == 0)
            return 1;
    }
    return 0;
}

// =========== EXPRESSION PARSING ===========

ASTNode *parsePrimary(Parser *parser) {
    if (!parser || !parser->current_token) return NULL;

    if (match(parser, TOKEN_INTEGER)) {
        ASTNode *node = createASTLiteral(NODE_INTEGER_LITERAL, parser->current_token->value);
        advance(parser);
        return node;
    }
    if (match(parser, TOKEN_FLOAT)) {
        ASTNode *node = createASTLiteral(NODE_FLOAT_LITERAL, parser->current_token->value);
        advance(parser);
        return node;
    }
    if (match(parser, TOKEN_STRING)) {
        ASTNode *node = createASTLiteral(NODE_STRING_LITERAL, parser->current_token->value);
        advance(parser);
        return node;
    }
    if (match(parser, TOKEN_IDENTIFIER)) {
        ASTNode *node = createASTLiteral(NODE_IDENTIFIER, parser->current_token->value);
        advance(parser);
        return node;
    }
    if (matchValue(parser, "(")) {
        advance(parser); // consume '('
        ASTNode *expr = parseExpression(parser);
        expextValue(parser, ")");
        return expr;
    }

    fprintf(stderr, "Syntax Error: Unexpected token '%s' in primary expression\n",
            parser->current_token->value ? parser->current_token->value : "(null)");
    return NULL;
}

ASTNode *parsePostfix(Parser *parser) {
    ASTNode *node = parsePrimary(parser);
    if (!node) return NULL;

    while (1) {
        if (matchValue(parser, "(")) {
            // Function call
            char *funcName = NULL;
            if (node->type == NODE_IDENTIFIER) {
                funcName = strdup(node->data.literal.value);
            } else {
                funcName = strdup("");
            }
            freeAST(node);

            advance(parser); // consume '('
            ASTNode *callNode = createASTNode(NODE_FUNC_CALL);
            callNode->data.funCall.name = funcName;
            callNode->data.funCall.args = NULL;
            callNode->data.funCall.argCount = 0;

            if (!matchValue(parser, ")")) {
                int capacity = 4;
                callNode->data.funCall.args = (ASTNode **)malloc(sizeof(ASTNode *) * capacity);
                while (1) {
                    ASTNode *arg = parseAssignment(parser);
                    if (arg) {
                        if (callNode->data.funCall.argCount >= capacity) {
                            capacity *= 2;
                            callNode->data.funCall.args = (ASTNode **)realloc(callNode->data.funCall.args, sizeof(ASTNode *) * capacity);
                        }
                        callNode->data.funCall.args[callNode->data.funCall.argCount++] = arg;
                    }
                    if (matchValue(parser, ",")) {
                        advance(parser);
                    } else {
                        break;
                    }
                }
            }
            expextValue(parser, ")");
            node = callNode;
        } else if (matchValue(parser, "[")) {
            advance(parser); // consume '['
            ASTNode *index = parseExpression(parser);
            expextValue(parser, "]");
            ASTNode *arrAccess = createASTNode(NODE_ARRAY_ACCESS);
            arrAccess->data.arrayAccess.array = node;
            arrAccess->data.arrayAccess.index = index;
            node = arrAccess;
        } else if (matchValue(parser, "++") || matchValue(parser, "--")) {
            char *op = strdup(parser->current_token->value);
            advance(parser);
            ASTNode *unaryNode = createASTNode(NODE_UNARY_OP);
            unaryNode->data.unaryOp.op = op;
            unaryNode->data.unaryOp.operand = node;
            node = unaryNode;
        } else {
            break;
        }
    }
    return node;
}

ASTNode *parseUnary(Parser *parser) {
    if (!parser || !parser->current_token) return NULL;

    if (matchValue(parser, "-") || matchValue(parser, "+") ||
        matchValue(parser, "!") || matchValue(parser, "~") ||
        matchValue(parser, "++") || matchValue(parser, "--") ||
        matchValue(parser, "&") || matchValue(parser, "*")) {
        char *op = strdup(parser->current_token->value);
        advance(parser);
        ASTNode *operand = parseUnary(parser);
        ASTNode *node = createASTUnaryOp(op, operand);
        free(op);
        return node;
    }

    return parsePostfix(parser);
}

ASTNode *parseMultiplicative(Parser *parser) {
    ASTNode *left = parseUnary(parser);
    if (!left) return NULL;

    while (matchValue(parser, "*") || matchValue(parser, "/") || matchValue(parser, "%")) {
        char *op = strdup(parser->current_token->value);
        advance(parser);
        ASTNode *right = parseUnary(parser);
        left = createASTBinaryOp(op, left, right);
        free(op);
    }
    return left;
}

ASTNode *parseAdditive(Parser *parser) {
    ASTNode *left = parseMultiplicative(parser);
    if (!left) return NULL;

    while (matchValue(parser, "+") || matchValue(parser, "-")) {
        char *op = strdup(parser->current_token->value);
        advance(parser);
        ASTNode *right = parseMultiplicative(parser);
        left = createASTBinaryOp(op, left, right);
        free(op);
    }
    return left;
}

ASTNode *parseRelational(Parser *parser) {
    ASTNode *left = parseAdditive(parser);
    if (!left) return NULL;

    while (matchValue(parser, "<") || matchValue(parser, "<=") ||
           matchValue(parser, ">") || matchValue(parser, ">=")) {
        char *op = strdup(parser->current_token->value);
        advance(parser);
        ASTNode *right = parseAdditive(parser);
        left = createASTBinaryOp(op, left, right);
        free(op);
    }
    return left;
}

ASTNode *parseEquality(Parser *parser) {
    ASTNode *left = parseRelational(parser);
    if (!left) return NULL;

    while (matchValue(parser, "==") || matchValue(parser, "!=")) {
        char *op = strdup(parser->current_token->value);
        advance(parser);
        ASTNode *right = parseRelational(parser);
        left = createASTBinaryOp(op, left, right);
        free(op);
    }
    return left;
}

ASTNode *parseLogicalAnd(Parser *parser) {
    ASTNode *left = parseEquality(parser);
    if (!left) return NULL;

    while (matchValue(parser, "&&")) {
        char *op = strdup(parser->current_token->value);
        advance(parser);
        ASTNode *right = parseEquality(parser);
        left = createASTBinaryOp(op, left, right);
        free(op);
    }
    return left;
}

ASTNode *parseLogicalOr(Parser *parser) {
    ASTNode *left = parseLogicalAnd(parser);
    if (!left) return NULL;

    while (matchValue(parser, "||")) {
        char *op = strdup(parser->current_token->value);
        advance(parser);
        ASTNode *right = parseLogicalAnd(parser);
        left = createASTBinaryOp(op, left, right);
        free(op);
    }
    return left;
}

ASTNode *parseAssignment(Parser *parser) {
    ASTNode *left = parseLogicalOr(parser);
    if (!left) return NULL;

    if (matchValue(parser, "=") || matchValue(parser, "+=") ||
        matchValue(parser, "-=") || matchValue(parser, "*=") ||
        matchValue(parser, "/=")) {
        char *op = strdup(parser->current_token->value);
        advance(parser);
        ASTNode *right = parseAssignment(parser);
        ASTNode *node = createASTNode(NODE_ASSIGNMENT);
        node->data.binaryOp.op = op;
        node->data.binaryOp.left = left;
        node->data.binaryOp.right = right;
        return node;
    }
    return left;
}

ASTNode *parseExpression(Parser *parser) {
    return parseAssignment(parser);
}

// =========== DECLARATIONS & STATEMENTS ===========

ASTNode *parseFunctionDecl(Parser *parser, char *returnType, char *name) {
    ASTNode *node = createASTNode(NODE_FUNC_DECL);
    node->data.funDecl.returnType = strdup(returnType);
    node->data.funDecl.name = strdup(name);
    node->data.funDecl.params = NULL;
    node->data.funDecl.paramCount = 0;
    node->data.funDecl.body = NULL;

    expextValue(parser, "(");

    if (!matchValue(parser, ")")) {
        if (matchValue(parser, "void") && peek(parser, 1) && strcmp(peek(parser, 1)->value, ")") == 0) {
            advance(parser); // consume 'void'
        } else {
            int capacity = 4;
            node->data.funDecl.params = (ASTNode **)malloc(sizeof(ASTNode *) * capacity);

            while (1) {
                if (!match(parser, TOKEN_KEYWORD) || !isDataType(parser->current_token->value)) {
                    fprintf(stderr, "Syntax Error: Expected parameter type, got '%s'\n",
                            parser->current_token->value ? parser->current_token->value : "(null)");
                    break;
                }
                char *pType = strdup(parser->current_token->value);
                advance(parser);

                while (matchValue(parser, "*")) {
                    char *newType = (char *)malloc(strlen(pType) + 2);
                    sprintf(newType, "%s*", pType);
                    free(pType);
                    pType = newType;
                    advance(parser);
                }

                char *pName = NULL;
                if (match(parser, TOKEN_IDENTIFIER)) {
                    pName = strdup(parser->current_token->value);
                    advance(parser);
                } else {
                    pName = strdup("");
                }

                ASTNode *paramNode = createASTNode(NODE_PARAM_DECL);
                paramNode->data.varDecl.dataType = pType;
                paramNode->data.varDecl.name = pName;
                paramNode->data.varDecl.init = NULL;

                if (node->data.funDecl.paramCount >= capacity) {
                    capacity *= 2;
                    node->data.funDecl.params = (ASTNode **)realloc(node->data.funDecl.params, sizeof(ASTNode *) * capacity);
                }
                node->data.funDecl.params[node->data.funDecl.paramCount++] = paramNode;

                if (matchValue(parser, ",")) {
                    advance(parser);
                } else {
                    break;
                }
            }
        }
    }

    expextValue(parser, ")");

    if (matchValue(parser, "{")) {
        node->data.funDecl.body = parseCompoundStmt(parser);
    } else if (matchValue(parser, ";")) {
        advance(parser);
        node->data.funDecl.body = NULL;
    } else {
        fprintf(stderr, "Syntax Error: Expected '{' or ';' after function header, got '%s'\n",
                parser->current_token->value ? parser->current_token->value : "(null)");
    }

    return node;
}

ASTNode *parseDeclaration(Parser *parser) {
    if (!parser || !parser->current_token) return NULL;

    if (!match(parser, TOKEN_KEYWORD) || !isDataType(parser->current_token->value)) {
        fprintf(stderr, "Syntax Error: Expected type specifier, got '%s'\n",
                parser->current_token->value ? parser->current_token->value : "(null)");
        return NULL;
    }

    char *dataType = strdup(parser->current_token->value);
    advance(parser);

    while (matchValue(parser, "*")) {
        char *newType = (char *)malloc(strlen(dataType) + 2);
        sprintf(newType, "%s*", dataType);
        free(dataType);
        dataType = newType;
        advance(parser);
    }

    if (!match(parser, TOKEN_IDENTIFIER)) {
        fprintf(stderr, "Syntax Error: Expected identifier after type '%s', got '%s'\n",
                dataType, parser->current_token->value ? parser->current_token->value : "(null)");
        free(dataType);
        return NULL;
    }

    char *name = strdup(parser->current_token->value);
    advance(parser);

    if (matchValue(parser, "(")) {
        ASTNode *func = parseFunctionDecl(parser, dataType, name);
        free(dataType);
        free(name);
        return func;
    }

    if (matchValue(parser, "[")) {
        advance(parser); // consume '['
        ASTNode *size = NULL;
        if (!matchValue(parser, "]")) {
            size = parseExpression(parser);
        }
        expextValue(parser, "]");

        ASTNode *init = NULL;
        if (matchValue(parser, "=")) {
            advance(parser);
            init = parseExpression(parser);
        }
        expextValue(parser, ";");

        ASTNode *node = createASTNode(NODE_ARRAY_DECL);
        node->data.arrayDecl.dataType = dataType;
        node->data.arrayDecl.name = name;
        node->data.arrayDecl.size = size;
        node->data.arrayDecl.init = init;
        return node;
    }

    ASTNode *init = NULL;
    if (matchValue(parser, "=")) {
        advance(parser);
        init = parseExpression(parser);
    }
    expextValue(parser, ";");

    ASTNode *node = createASTVarDecl(dataType, name, init);
    free(dataType);
    free(name);
    return node;
}

ASTNode *parseCompoundStmt(Parser *parser) {
    if (!matchValue(parser, "{")) {
        fprintf(stderr, "Syntax Error: Expected '{' to begin compound statement, got '%s'\n",
                parser->current_token->value ? parser->current_token->value : "(null)");
        return NULL;
    }
    advance(parser); // consume '{'

    ASTNode *node = createASTNode(NODE_COMPOUND_STMT);
    int capacity = 8;
    node->data.compoundStmt.statements = (ASTNode **)malloc(sizeof(ASTNode *) * capacity);
    node->data.compoundStmt.count = 0;
    node->data.compoundStmt.capacity = capacity;

    while (!matchValue(parser, "}") && !isAtEnd(parser)) {
        ASTNode *stmt = NULL;
        if (match(parser, TOKEN_KEYWORD) && isDataType(parser->current_token->value)) {
            stmt = parseDeclaration(parser);
        } else {
            stmt = parseStatement(parser);
        }

        if (stmt) {
            if (node->data.compoundStmt.count >= capacity) {
                capacity *= 2;
                node->data.compoundStmt.statements = (ASTNode **)realloc(node->data.compoundStmt.statements, sizeof(ASTNode *) * capacity);
                node->data.compoundStmt.capacity = capacity;
            }
            node->data.compoundStmt.statements[node->data.compoundStmt.count++] = stmt;
        } else {
            if (!matchValue(parser, "}") && !isAtEnd(parser)) {
                advance(parser);
            }
        }
    }

    expextValue(parser, "}");
    return node;
}

ASTNode *parseIfStmt(Parser *parser) {
    expextValue(parser, "if");
    expextValue(parser, "(");
    ASTNode *condition = parseExpression(parser);
    expextValue(parser, ")");
    ASTNode *thenBranch = parseStatement(parser);
    ASTNode *elseBranch = NULL;

    if (matchValue(parser, "else")) {
        advance(parser);
        elseBranch = parseStatement(parser);
    }

    ASTNode *node = createASTNode(NODE_IF_STMT);
    node->data.ifStmt.condition = condition;
    node->data.ifStmt.thenBranch = thenBranch;
    node->data.ifStmt.elseBranch = elseBranch;
    return node;
}

ASTNode *parseWhileStmt(Parser *parser) {
    expextValue(parser, "while");
    expextValue(parser, "(");
    ASTNode *condition = parseExpression(parser);
    expextValue(parser, ")");
    ASTNode *body = parseStatement(parser);

    ASTNode *node = createASTNode(NODE_WHILE_STMT);
    node->data.loop.condition = condition;
    node->data.loop.body = body;
    node->data.loop.init = NULL;
    node->data.loop.increment = NULL;
    return node;
}

ASTNode *parseForStmt(Parser *parser) {
    expextValue(parser, "for");
    expextValue(parser, "(");

    ASTNode *init = NULL;
    if (matchValue(parser, ";")) {
        advance(parser);
    } else if (match(parser, TOKEN_KEYWORD) && isDataType(parser->current_token->value)) {
        init = parseDeclaration(parser);
    } else {
        init = parseExpressionStmt(parser);
    }

    ASTNode *condition = NULL;
    if (!matchValue(parser, ";")) {
        condition = parseExpression(parser);
    }
    expextValue(parser, ";");

    ASTNode *increment = NULL;
    if (!matchValue(parser, ")")) {
        increment = parseExpression(parser);
    }
    expextValue(parser, ")");

    ASTNode *body = parseStatement(parser);

    ASTNode *node = createASTNode(NODE_FOR_STMT);
    node->data.loop.init = init;
    node->data.loop.condition = condition;
    node->data.loop.increment = increment;
    node->data.loop.body = body;
    return node;
}

ASTNode *parseReturnStmt(Parser *parser) {
    expextValue(parser, "return");
    ASTNode *expr = NULL;
    if (!matchValue(parser, ";")) {
        expr = parseExpression(parser);
    }
    expextValue(parser, ";");

    ASTNode *node = createASTNode(NODE_RETURN_STMT);
    node->data.returnStmt.expr = expr;
    return node;
}

ASTNode *parseExpressionStmt(Parser *parser) {
    if (matchValue(parser, ";")) {
        advance(parser);
        return NULL;
    }

    ASTNode *expr = parseExpression(parser);
    expextValue(parser, ";");

    ASTNode *node = createASTNode(NODE_EXPRESSION_STMT);
    node->data.exprStmt.expr = expr;
    return node;
}

ASTNode *parseStatement(Parser *parser) {
    if (!parser || !parser->current_token) return NULL;

    if (matchValue(parser, "{")) {
        return parseCompoundStmt(parser);
    }
    if (matchValue(parser, "if")) {
        return parseIfStmt(parser);
    }
    if (matchValue(parser, "while")) {
        return parseWhileStmt(parser);
    }
    if (matchValue(parser, "for")) {
        return parseForStmt(parser);
    }
    if (matchValue(parser, "return")) {
        return parseReturnStmt(parser);
    }
    if (matchValue(parser, "break")) {
        advance(parser);
        expextValue(parser, ";");
        return createASTNode(NODE_BREAK_STMT);
    }
    if (matchValue(parser, "continue")) {
        advance(parser);
        expextValue(parser, ";");
        return createASTNode(NODE_CONTINUE_STMT);
    }
    if (match(parser, TOKEN_KEYWORD) && isDataType(parser->current_token->value)) {
        return parseDeclaration(parser);
    }
    return parseExpressionStmt(parser);
}

// =========== PROGRAM ROOT PARSING ===========

ASTNode *parseProgram(Parser *parser) {
    if (!parser) return NULL;

    ASTNode *root = createASTNode(NODE_PROGRAM);
    int capacity = 16;
    root->data.program.declarations = (ASTNode **)malloc(sizeof(ASTNode *) * capacity);
    root->data.program.count = 0;
    root->data.program.capacity = capacity;

    while (!isAtEnd(parser)) {
        ASTNode *decl = NULL;
        if (match(parser, TOKEN_KEYWORD) && isDataType(parser->current_token->value)) {
            decl = parseDeclaration(parser);
        } else {
            decl = parseStatement(parser);
        }

        if (decl) {
            if (root->data.program.count >= capacity) {
                capacity *= 2;
                root->data.program.declarations = (ASTNode **)realloc(root->data.program.declarations, sizeof(ASTNode *) * capacity);
                root->data.program.capacity = capacity;
            }
            root->data.program.declarations[root->data.program.count++] = decl;
        } else {
            if (!isAtEnd(parser)) {
                advance(parser);
            }
        }
    }

    return root;
}

// =========== AST VISUALIZATION ===========

static void printIndent(int level) {
    for (int i = 0; i < level; i++) {
        printf("  ");
    }
}

void printAST(ASTNode *node, int level) {
    if (!node) return;

    printIndent(level);

    switch (node->type) {
        case NODE_PROGRAM:
            printf("Program\n");
            for (int i = 0; i < node->data.program.count; i++) {
                printAST(node->data.program.declarations[i], level + 1);
            }
            break;

        case NODE_FUNC_DECL:
            printf("FunctionDecl: %s %s\n", node->data.funDecl.returnType, node->data.funDecl.name);
            if (node->data.funDecl.paramCount > 0) {
                printIndent(level + 1);
                printf("Parameters:\n");
                for (int i = 0; i < node->data.funDecl.paramCount; i++) {
                    printAST(node->data.funDecl.params[i], level + 2);
                }
            }
            if (node->data.funDecl.body) {
                printIndent(level + 1);
                printf("Body:\n");
                printAST(node->data.funDecl.body, level + 2);
            }
            break;

        case NODE_PARAM_DECL:
            printf("Param: %s %s\n", node->data.varDecl.dataType, node->data.varDecl.name);
            break;

        case NODE_VAR_DECL:
            printf("VarDecl: %s %s\n", node->data.varDecl.dataType, node->data.varDecl.name);
            if (node->data.varDecl.init) {
                printIndent(level + 1);
                printf("Init:\n");
                printAST(node->data.varDecl.init, level + 2);
            }
            break;

        case NODE_ARRAY_DECL:
            printf("ArrayDecl: %s %s\n", node->data.arrayDecl.dataType, node->data.arrayDecl.name);
            if (node->data.arrayDecl.size) {
                printIndent(level + 1);
                printf("Size:\n");
                printAST(node->data.arrayDecl.size, level + 2);
            }
            if (node->data.arrayDecl.init) {
                printIndent(level + 1);
                printf("Init:\n");
                printAST(node->data.arrayDecl.init, level + 2);
            }
            break;

        case NODE_COMPOUND_STMT:
            printf("CompoundStmt\n");
            for (int i = 0; i < node->data.compoundStmt.count; i++) {
                printAST(node->data.compoundStmt.statements[i], level + 1);
            }
            break;

        case NODE_IF_STMT:
            printf("IfStmt\n");
            printIndent(level + 1);
            printf("Condition:\n");
            printAST(node->data.ifStmt.condition, level + 2);
            printIndent(level + 1);
            printf("Then:\n");
            printAST(node->data.ifStmt.thenBranch, level + 2);
            if (node->data.ifStmt.elseBranch) {
                printIndent(level + 1);
                printf("Else:\n");
                printAST(node->data.ifStmt.elseBranch, level + 2);
            }
            break;

        case NODE_WHILE_STMT:
            printf("WhileStmt\n");
            printIndent(level + 1);
            printf("Condition:\n");
            printAST(node->data.loop.condition, level + 2);
            printIndent(level + 1);
            printf("Body:\n");
            printAST(node->data.loop.body, level + 2);
            break;

        case NODE_FOR_STMT:
            printf("ForStmt\n");
            if (node->data.loop.init) {
                printIndent(level + 1);
                printf("Init:\n");
                printAST(node->data.loop.init, level + 2);
            }
            if (node->data.loop.condition) {
                printIndent(level + 1);
                printf("Condition:\n");
                printAST(node->data.loop.condition, level + 2);
            }
            if (node->data.loop.increment) {
                printIndent(level + 1);
                printf("Increment:\n");
                printAST(node->data.loop.increment, level + 2);
            }
            if (node->data.loop.body) {
                printIndent(level + 1);
                printf("Body:\n");
                printAST(node->data.loop.body, level + 2);
            }
            break;

        case NODE_RETURN_STMT:
            printf("ReturnStmt\n");
            if (node->data.returnStmt.expr) {
                printAST(node->data.returnStmt.expr, level + 1);
            }
            break;

        case NODE_EXPRESSION_STMT:
            printf("ExprStmt\n");
            if (node->data.exprStmt.expr) {
                printAST(node->data.exprStmt.expr, level + 1);
            }
            break;

        case NODE_BREAK_STMT:
            printf("BreakStmt\n");
            break;

        case NODE_CONTINUE_STMT:
            printf("ContinueStmt\n");
            break;

        case NODE_ASSIGNMENT:
            printf("Assignment (%s)\n", node->data.binaryOp.op);
            printAST(node->data.binaryOp.left, level + 1);
            printAST(node->data.binaryOp.right, level + 1);
            break;

        case NODE_BINARY_OP:
            printf("BinaryOp (%s)\n", node->data.binaryOp.op);
            printAST(node->data.binaryOp.left, level + 1);
            printAST(node->data.binaryOp.right, level + 1);
            break;

        case NODE_UNARY_OP:
            printf("UnaryOp (%s)\n", node->data.unaryOp.op);
            printAST(node->data.unaryOp.operand, level + 1);
            break;

        case NODE_FUNC_CALL:
            printf("FuncCall: %s\n", node->data.funCall.name);
            for (int i = 0; i < node->data.funCall.argCount; i++) {
                printAST(node->data.funCall.args[i], level + 1);
            }
            break;

        case NODE_ARRAY_ACCESS:
            printf("ArrayAccess\n");
            printIndent(level + 1);
            printf("Array:\n");
            printAST(node->data.arrayAccess.array, level + 2);
            printIndent(level + 1);
            printf("Index:\n");
            printAST(node->data.arrayAccess.index, level + 2);
            break;

        case NODE_INTEGER_LITERAL:
            printf("IntegerLiteral: %s\n", node->data.literal.value);
            break;

        case NODE_FLOAT_LITERAL:
            printf("FloatLiteral: %s\n", node->data.literal.value);
            break;

        case NODE_STRING_LITERAL:
            printf("StringLiteral: \"%s\"\n", node->data.literal.value);
            break;

        case NODE_IDENTIFIER:
            printf("Identifier: %s\n", node->data.literal.value);
            break;

        default:
            printf("UnknownNode (%d)\n", node->type);
            break;
    }
}
