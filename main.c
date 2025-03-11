#include <stdio.h>
// #include <stdlib.h>
int main() {
    // char *filename= malloc(256);  // allocating 256 bytes to the file name

    // if (filename == NULL) {   //checking if memory was allocated successfully
    //     printf("Memory allocation failed.\n");
    //     return 1;       //convention in C to represent an error
    // }

    // // Ask the user for the file's name
    // printf("Please enter the file name: ");
    // scanf("%255s", filename);

    // Open the file in read mode ("r")
    char filename[] = "test.txt";
    FILE *file = fopen(filename, "r");
    // free(filename);
    if (file == NULL) {  // displays error if cannot open file
        printf("Error in opening file: %s", filename);
        return 1;       //convention in C to represent an error
    }

    // Read each character in each line, and displays it one at a time
    char c;
    while ((c=fgetc(file)) != EOF) {
        printf("%c", c);
    }

    // Close the file
    fclose(file);

    return 0;       //convention in C to represent successful execution   
}
