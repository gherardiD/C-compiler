#include <stdio.h>
#include <stdlib.h>
#include "lexer.h"


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
    Token *tokens = analize(buffer);
    free(buffer);
    fclose(filePointer);
    return 0;

}
