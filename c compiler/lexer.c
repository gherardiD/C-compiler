#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#define MAX_LENGTH 100
#define MAX_TOKENS 1000

typedef enum {
    TOKEN_KEYWORD,
    TOKEN_IDENTIFIER,
    TOKEN_INTEGER,
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
int analize(char *input);
Token makeToken(TokenType type, const char *value);
void printToken(Token token);
void freeToken(Token token);


Token makeToken(TokenType type, const char *value) {
    Token token;
    token.type = type;
    token.value = strdup(value);  // allocate string
    return token;
}

void printToken(Token token) {
    const char *names[] = {
        "Keyword", "Identifier", "Integer", "Operator", "Punctuator", "String", "Invalid"
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
    if (!isalpha(str[0]))
        return 0;
    for (int i = 1; str[i] != '\0'; i++){
        if (!isalnum(str[i]))
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
    if (ch == '+' || ch == '-' || ch == '*' ||
        ch == '/' || ch == '>' || ch == '<' ||
        ch == '=') {
        return 1;
    }
    return 0;
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


// while there isnt any delimiter, keep moving right
// when you find a delimiter, check if left != right, if so, get the substring from left to right-1
// check if the substring is a keyword, identifier, integer or invalid
// otherwise, check if the delimiter is an operator or punctuator
int analize(char *input) {
    int left, right = 0;
    int len = strlen(input);
    while (right <= len) {

        // GETSTRING FUNCTION ALREADY UPDATES left AND right
        // AND ALREADY MEAKES THE TOKEN AND RETURNS IT
        Token t;
        if (input[right] == '"' || input[right] == '\'') {
            t = getString(input, &left, &right);
            printToken(t);
            freeToken(t);
            continue;
        }

        // SKIP COMMENTS, I CAN ALSO MAKE A TOKEN FOR THEM IF NEEDED
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
            else if (isInteger(subStr))
                t = makeToken(TOKEN_INTEGER, subStr);
            else if (isValidIdentifier(subStr))
                t = makeToken(TOKEN_IDENTIFIER, subStr);
            else
                t = makeToken(TOKEN_INVALID, subStr);

            printToken(t);
            freeToken(t);
            free(subStr);

        }

        if (isOperator(input[right])){
            int opLength = getOperatorLength(input[right], input[right + 1]);
            if (opLength == 2){
                // struct token t = {.type = OPERATOR, .value = (char[]){input[right], input[right + 1], '\0'}};
                printf("Token: Operator, Value: %c%c\n", input[right], input[right + 1]);
                right++;
            }
            else{
                // struct token t = {.type = OPERATOR, .value = (char[]){input[right], '\0'}};
                printf("Token: Operator, Value: %c\n", input[right]);
            }
        }
        else if (isPunctuator(input[right])){
            // struct token t = {.type = PUNCTUATOR, .value = input[right]};
            printf("Token: Punctuator, Value: %c\n", input[right]);
        }

        right ++;
        left = right;
    }

    return 0;
}


// THE FIRST LETTER OF THE FILE IS NOT PRINTED, WHY?
int main(int argc, char *argv[]) {
    FILE *filePointer;

    // Open the file passed as a command line argument
    filePointer = fopen(argv[1], "r");
    if (filePointer == NULL) {
        printf("Could not open file %s\n", argv[1]);
        return 1;
    }

    fseek(filePointer, 0, SEEK_END);    // Go to end of file
    long size = ftell(filePointer);     // Get the size of the file
    rewind(filePointer);                // Go back to the beginning of the file
    char *buffer = malloc(size + 1);
    fread(buffer, 1, size, filePointer);
    buffer[size] = '\0';
    analize(buffer);
    printf("%c", buffer[0]);
    free(buffer);
    fclose(filePointer);
    return 0;

}
