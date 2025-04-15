#include "FileReader.h"
#include <stdio.h>
#include <string.h>

#define MAIN_MEMORY_SIZE 2048
#define WORD_SIZE 32
#define REGISTER_COUNT 33
#define MAX_LINES 64
#define MAX_INSTRUCTION_TOKENS 256


char* filepath = "test.txt";

struct Word {
    __uint32_t word; //32 bits to store full word
};

char lines[MAX_LINES][MAX_INSTRUCTION_TOKENS]; //An array to hold the text instructions after reading from file

struct Word mainMemory[MAIN_MEMORY_SIZE];
struct Word registers[REGISTER_COUNT];
struct Word programCounter;

void readFileToMemory(char* filepath);
void initRegisters();
void initMemory();
void initPipeline();
void fetch();
void decode();
void execute();
void memory();
void writeBack();

int main(){


    // char* textInstructions = readFile(filepath);
    // printf("%s\n", textInstructions);
    readFileToMemory(filepath);

    for(int i = 0; i < MAX_LINES; i++){
        printf("%s\n", lines[i]);
        if(strcmp(lines[i], "[END]") == 0) break;
    }

}

void readFileToMemory(char* filepath){

    char* textInstructions = readFile(filepath);
    // printf("%s\n", textInstructions);
    int lineCount = 0;

    char* token = strtok(textInstructions, "\n");
    while(token != NULL && lineCount < MAX_LINES){
        strncpy(lines[lineCount], token, MAX_INSTRUCTION_TOKENS - 1);
        lines[lineCount][MAX_INSTRUCTION_TOKENS - 1] = '\0';
        lineCount++;
        token = strtok(NULL, "\n");
    }

    strncpy(lines[lineCount], "[END]", MAX_INSTRUCTION_TOKENS - 1);



}