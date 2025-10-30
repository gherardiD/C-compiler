#ifndef LEXER_H
#define LEXER_H

#define MAX_LENGTH 100
#define MAX_TOKENS 1000

typedef enum {
    TOKEN_KEYWORD,
    TOKEN_IDENTIFIER,
    TOKEN_INTEGER,
    TOKEN_FLOAT,
    TOKEN_OPERATOR,
    TOKEN_PUNCTUATOR,
    TOKEN_STRING,
    TOKEN_INVALID
} TokenType;

typedef struct {
    TokenType type;
    char *value;
} Token;

int isKeyword(char *str);
int isValidIdentifier(char *str);
int isDelimiter(char ch);
int isOperator(char ch);
int isPunctuator(char ch);
int isInteger(char *str);
int isFloat(char *str);
Token * analize(char *input);
Token makeToken(TokenType type, const char *value);
void printToken(Token token);
void freeToken(Token token);


#endif
