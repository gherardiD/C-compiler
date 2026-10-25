#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"
#include "parser.h"
#include "semantic.h"

int main(int argc, char *argv[]) {
    const char *filepath = (argc > 1) ? argv[1] : "test.txt";
    FILE *filePointer;

    // Open the file passed as a command line argument (or default test.txt)
    filePointer = fopen(filepath, "r");
    if (filePointer == NULL) {
        printf("Could not open file %s\n", filepath);
        return 1;
    }

    fseek(filePointer, 0, SEEK_END);    // Go to end of file
    long size = ftell(filePointer);     // Get the size of the file
    rewind(filePointer);                // Go back to the beginning of the file
    char *buffer = malloc(size + 1);
    fread(buffer, 1, size, filePointer);
    buffer[size] = '\0';
    fclose(filePointer);

    printf("=== LEXER TOKENS ===\n");
    Token *tokens = analize(buffer);
    free(buffer);

    printf("\n=== PARSER AST ===\n");
    Parser *parser = createParser(tokens, 0);
    ASTNode *ast = parseProgram(parser);
    printAST(ast, 0);

    printf("\n=== SEMANTIC ANALYSIS ===\n");
    SemanticAnalyzer *analyzer = createSemanticAnalyzer();
    int errors = analyzeProgram(analyzer, ast);
    if (errors == 0) {
        printf("Semantic analysis completed successfully with 0 errors.\n");
        printf("\n=== GLOBAL SYMBOL TABLE ===\n");
        printSymbolTable(analyzer);
    } else {
        printf("Semantic analysis completed with %d error(s).\n", errors);
    }

    // Cleanup
    freeSemanticAnalyzer(analyzer);
    freeAST(ast);
    freeParser(parser);

    int i = 0;
    while (tokens[i].value != NULL) {
        int isEof = (tokens[i].type == TOKEN_INVALID && strcmp(tokens[i].value, "EOF") == 0);
        freeToken(tokens[i]);
        if (isEof) break;
        i++;
    }
    free(tokens);

    return 0;
}
