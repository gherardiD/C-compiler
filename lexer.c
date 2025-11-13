#include "lexer.h"
#include <stdio.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

Token makeToken(TokenType type, const char *value) {
    Token token;
    token.type = type;
    token.value = strdup(value);  // allocate string, returns pointer to new string
    return token;
}

void printToken(Token token) {
    const char *names[] = {
        "Keyword", "Identifier", "Integer", "Float", "Operator", "Punctuator", "String", "Invalid"
    };
    printf("Token: %s, Value: %s\n", names[token.type], token.value);
}

void freeToken(Token token) {
    free(token.value);
}


int isKeyword(char *str){
    const char* keywords[]
        = { "auto",     "break",    "case",     "char",
            "const",    "continue", "default",  "do",
            "double",   "else",     "enum",     "extern",
            "float",    "for",      "goto",     "if",
            "int",      "long",     "register", "return",
            "short",    "signed",   "sizeof",   "static",
            "struct",   "switch",   "typedef",  "union",
            "unsigned", "void",     "volatile", "while" };
    int len = sizeof(keywords) / sizeof(keywords[0]);
    for (int i = 0; i < len; i++){
        if (strcmp(str, keywords[i]) == 0)
            return 1;
    }
    return 0;
}

int isValidIdentifier(char *str) {
    if (!isalpha(str[0]) && str[0] != '_')
        return 0;
    for (int i = 1; str[i] != '\0'; i++){
        if (!isalnum(str[i]) && str[i] != '_')
            return 0;
    }
    return !isKeyword(str);
}

int isDelimiter(char ch) {
    if (ch == ' ' || ch == '+' || ch == '-' || ch == '*' ||
        ch == '/' || ch == ',' || ch == ';' || ch == '>' ||
        ch == '<' || ch == '=' || ch == '(' || ch == ')' ||
        ch == '[' || ch == ']' || ch == '{' || ch == '}' ||
        ch == '\n' || ch == '\t') {
        return 1;
    }
    return 0;
}

int isOperator(char ch) {
    return ch == '+' || ch == '-' || ch == '*' ||
           ch == '/' || ch == '=' || ch == '<' ||
           ch == '>' || ch == '!' || ch == '&' ||
           ch == '|' || ch == '^' || ch == '%';
}

int getOperatorLength(char c1, char c2) {
    char *twoCharOps[] = {"++", "--", "+=", "-=", "*=", "/=", "==", "!=", "<=", ">="};
    for (int i = 0; i < 10; i++) {
        if (twoCharOps[i][0] == c1 && twoCharOps[i][1] == c2)
            return 2;
    }
    return 1;
}


int isPunctuator(char ch) {
    if (ch == ',' || ch == ';' || ch == '(' || ch == ')' ||
        ch == '{' || ch == '}' || ch == '[' || ch == ']') {
        return 1;
    }
    return 0;
}


int isInteger(char *str) {
    int i = 0;
    if (str[i] == '+' || str[i] == '-') i++;
    if (!isdigit(str[i])) return 0;
    for (; str[i]; i++) if (!isdigit(str[i])) return 0;
    return 1;
}


int isFloat(char *str){
    int i = 0;
    int hasDigit = 0;
    int hasDot = 0;

    if (str[i] == '+' || str[i] == '-') i++;

    while(isdigit(str[i])){
        hasDigit = 1;
        i++;
    }

    if (str[i] == '.'){
        hasDot = 1;
        i++;
        while(isdigit(str[i])){
            hasDigit = 1;
            i++;
        }
    }

    if (!hasDigit || !hasDot) return 0;

    return str[i] == '\0';
}


Token getString(char *input, int *left, int *right) {
    char quoteType = input[*right];
    (*right)++;
    int start = *right;

    while (input[*right] != quoteType && input[*right] != '\0') {
        if (input[*right] == '\\' && input[*right + 1] == quoteType) {
            (*right) += 2;
        } else {
            (*right)++;
        }
    }

    int end = *right - 1;
    // if (input[*right] == '\0') {
    //     return makeToken(TOKEN_INVALID, "Unterminated string literal");
    // }

    char *subStr = (char*)malloc((end - start + 2) * sizeof(char));
    for (int i = start; i <= end; i++) {
        subStr[i - start] = input[i];
    }
    subStr[end - start + 1] = '\0';

    Token t = makeToken(TOKEN_STRING, subStr);
    free(subStr);

    (*right)++;
    *left = *right;
    return t;
}

char *getSubString(char *str, int left, int right) {
    int i;
    char *subStr = (char*)malloc((right - left + 2) * sizeof(char));
    for (i = left; i <= right; i++){
        subStr[i - left] = str[i];
    }
    subStr[right - left + 1] = '\0';
    return subStr;
}


Token * analize(char *input) {
    int left = 0, right = 0;
    int len = strlen(input);


    // If you want to make it scalable,
    // you can replace MAX_TOKENS with a dynamic resize using realloc(), like this:
    // if (tokenCount >= capacity) {
    //     capacity *= 2;
    //     tokens = realloc(tokens, sizeof(Token) * capacity);
    // }

    Token *tokens = (Token *)malloc(sizeof(Token) * MAX_TOKENS);
    int tokenCount = 0;

    while (right <= len) {

        // GETSTRING FUNCTION ALREADY UPDATES left AND right
        // AND ALREADY MEAKES THE TOKEN AND RETURNS IT
        Token t;
        if (input[right] == '"' || input[right] == '\'') {
            t = getString(input, &left, &right);
            tokens[tokenCount++] = t;
            printToken(t);
            continue;
        }

        // skip comments
        if (input[right] == '/' && right + 1 < len && input[right + 1] == '/'){
            while (input[right] != '\n' && right < len){
                right++;
            }
            left = right;
            continue;
        }else if (input[right] == '/' && right + 1 < len && input[right + 1] == '*'){
            right += 2;
            while (!(input[right] == '*' && input[right + 1] == '/') && right < len){
                right++;
            }
            right += 2;
            left = right;
            continue;
        }

        // at this point right is at a delimiter
        if (!isDelimiter(input[right])) {
            right++;
            continue;
        }

        if (left != right) {
            char *subStr = (char*)getSubString(input, left, right - 1);

            if (isKeyword(subStr))
                t = makeToken(TOKEN_KEYWORD, subStr);
            else if(isFloat(subStr))
                t = makeToken(TOKEN_FLOAT, subStr);
            else if (isInteger(subStr))
                t = makeToken(TOKEN_INTEGER, subStr);
            else if (isValidIdentifier(subStr))
                t = makeToken(TOKEN_IDENTIFIER, subStr);
            else
                t = makeToken(TOKEN_INVALID, subStr);

            tokens[tokenCount++] = t;
            printToken(t);
            free(subStr);

        }

        if (isOperator(input[right])){
            int opLength = getOperatorLength(input[right], input[right + 1]);
            char *val = (char*)malloc(opLength + 1);

            if (opLength == 2){
                val[0] = input[right];
                val[1] = input[right + 1];
                val[2] = '\0';
                right++;
            }
            else{
                val[0] = input[right];
                val[1] = '\0';
            }

            t = makeToken(TOKEN_OPERATOR, val);
            tokens[tokenCount++] = t;
            printToken(t);
            free(val);

        }
        else if (isPunctuator(input[right])){
            char val[2] = {input[right], '\0'};
            t = makeToken(TOKEN_PUNCTUATOR, val);
            tokens[tokenCount++] = t;
            printToken(t);
        }

        right ++;
        left = right;
    }

    tokens[tokenCount] = makeToken(TOKEN_INVALID, "EOF");

    return tokens;
}
