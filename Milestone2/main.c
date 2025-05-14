#include "FileReader.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAIN_MEMORY_SIZE 2048
#define WORD_SIZE 32
#define REGISTER_COUNT 33
#define MAX_LINES 64
#define MAX_INSTRUCTION_TOKENS 256

char* filepath = "test.txt";

enum InstructionFormat {
    R_FORMAT,
    I_FORMAT,
    J_FORMAT,
    INVALID_FORMAT
};

struct Word {
    int word; //32 bits to store full word
    enum InstructionFormat format;
};

struct Pipeline {
    struct Word fetchPhaseInst;
    struct Word decodePhaseInst;
    struct Word executePhaseInst;
    struct Word memoryPhaseInst;
    struct Word writebackPhaseInst;
};



char lines[MAX_LINES][MAX_INSTRUCTION_TOKENS]; //An array to hold the text instructions after reading from file
int lineCount = 0;

struct Word mainMemory[MAIN_MEMORY_SIZE];
struct Word registers[REGISTER_COUNT];
struct Word programCounter;
struct Pipeline pipeline;

void printBinary(unsigned int num) {

    for (int i = 31; i >= 0; i--) {
        printf("%d", (num >> i) & 1);
        if (i % 4 == 0) printf(" "); // optional spacing every 4 bits
    }
    printf("\n");
}

void printBinaryR(unsigned int num) {

    for (int i = 31; i >= 0; i--) {
        printf("%d", (num >> i) & 1);
        if (i == 32-4 || i == 32-9 || i == 32-14 || i == 32-19) printf(" "); // optional spacing every 4 bits
    }
    printf("\n");
}

void printBinaryI(unsigned int num) {

    for (int i = 31; i >= 0; i--) {
        printf("%d", (num >> i) & 1);
        if (i == 32-4 || i == 32-9 || i == 32-14) printf(" "); // optional spacing every 4 bits
    }
    printf("\n");
}

void printBinaryJ(unsigned int num) {

    for (int i = 31; i >= 0; i--) {
        printf("%d", (num >> i) & 1);
        if (i == 32-4) printf(" "); // optional spacing every 4 bits
    }
    printf("\n");
}

void printMainMemory(){
    for(int i = 0; i < lineCount; i++){
        switch (mainMemory[i].format)
        {
            case R_FORMAT:
            printBinaryR(mainMemory[i].word);
            break;
            case I_FORMAT:
            printBinaryI(mainMemory[i].word);
            break;
            case J_FORMAT:
            printBinaryJ(mainMemory[i].word);
            break;
        
        default:
            break;
        }
    }
}


void parseTextInstruction(); // Parses the text instructions into their binary representation.
void readFileToMemory(char* filepath);
void initRegisters();
void initMemory();
void initPipeline();
void fetchAddress(int pcAddress){
    if (pcAddress < lineCount){
       if (pcAddress%2 == 0){
        pipeline.fetchPhaseInst = mainMemory[pcAddress];
       } //we only fetch every other cycle
    } else {
        pipeline.fetchPhaseInst = (struct Word) { .word = 0, .format = INVALID_FORMAT};
    }
}
void fetch(){
    if (programCounter.word < lineCount){
       if (programCounter.word%2 == 0){
        pipeline.fetchPhaseInst = mainMemory[programCounter.word];
       } //we only fetch every other cycle
    } else {
        pipeline.fetchPhaseInst = (struct Word) { .word = 0, .format = INVALID_FORMAT};
    }
}
void decode(){
    pipeline.decodePhaseInst = pipeline.fetchPhaseInst;
}
void execute(){
    pipeline.executePhaseInst = pipeline.decodePhaseInst;
}
void memory(){
    pipeline.memoryPhaseInst = pipeline.executePhaseInst;
}
void writeBack(){
    if (programCounter.word%2 == 0){
        pipeline.writebackPhaseInst = pipeline.executePhaseInst;
    } else {
        pipeline.writebackPhaseInst = pipeline.memoryPhaseInst;
    }
}
void handleHazards();

struct InstructionParts {
    char opcode[4];
    int rd, rs, rt, imm, shamt, address;
    enum InstructionFormat format;
};

void parseInstruction(struct Word instruction){
    int opcode = 0;
    int r1 = 0;
    int r2 = 0;
    int r3 = 0;
    int shamt = 0;
    int imm = 0;
    int address = 0;
    struct Word r1val;
    struct Word r2val;
    struct Word r3val;
    
    opcode = (instruction.word & 0xF0000000)>>28;
    r1 = (instruction.word & 0x0F800000)>>23;
    r2 = (instruction.word & 0x007C0000)>>18;
    r3 = (instruction.word & 0x0003E000)>>13;
    shamt = (instruction.word & 0x00001FFF);
    imm = (instruction.word & 0x0003FFFF);
    address = (instruction.word & 0x0FFFFFFF);  
    // TODO: assign register values


};

int main(){

    readFileToMemory(filepath);

    for(int i = 0; i < MAX_LINES; i++){
        printf("%s\n", lines[i]);
        if(strcmp(lines[i], "[END]") == 0) break;
    }

    parseTextInstruction();

    printMainMemory();

    // printf("\nLine Count: %d", lineCount);

}

void initRegisters(){
    for(int i = 0; i < REGISTER_COUNT; i++){
        struct Word word;
        word.word = 0;
        word.format = R_FORMAT;
        registers[i] = word;
    }
}


void initMemory(){
    for(int i = 0; i < MAIN_MEMORY_SIZE; i++){
        struct Word word;
        word.word = 0;
        word.format = R_FORMAT;
        mainMemory[i] = word;
    }
}


void initPipeline(){

    pipeline.fetchPhaseInst = (struct Word) {0};
    pipeline.decodePhaseInst = (struct Word) {0};
    pipeline.executePhaseInst= (struct Word) {0};
    pipeline.memoryPhaseInst = (struct Word) {0};
    pipeline.writebackPhaseInst = (struct Word) {0};

}



void parseTextInstruction(){

    char tokens[4][10];

    for(int i = 0; i < lineCount; i++){

        //Get Tokens Here

        int tokenIndex = 0;
        char* token = strtok(lines[i], " ");
        while(token != NULL){
            
            strncpy(tokens[tokenIndex], token, 10);
            tokenIndex++;
            token = strtok(NULL, " ");
        }

        //Convert To Binary Format Here

        char* opcode = tokens[0];
        struct Word binaryInstruction;
        binaryInstruction.word = 0;
        int opcodeBin = 0;
        enum InstructionFormat format;
        
        if (strcmp(opcode, "ADD") == 0) {
            opcodeBin = 0; 
            format = R_FORMAT;
        } else if (strcmp(opcode, "SUB") == 0) {
            opcodeBin = 1;
            format = R_FORMAT;
        } else if (strcmp(opcode, "MULI") == 0) {
            opcodeBin = 2;
            format = I_FORMAT;
        } else if (strcmp(opcode, "ADDI") == 0) {
            opcodeBin = 3;
            format = I_FORMAT;
        } else if (strcmp(opcode, "BNE") == 0) {
            opcodeBin = 4;
            format = I_FORMAT;
        } else if (strcmp(opcode, "ANDI") == 0) {
            opcodeBin = 5;
            format = I_FORMAT;
        } else if (strcmp(opcode, "ORI") == 0) {
            opcodeBin = 6;
            format = I_FORMAT;
        } else if (strcmp(opcode, "J") == 0) {
            opcodeBin = 7;
            format = J_FORMAT;
        } else if (strcmp(opcode, "SLL") == 0) {
            opcodeBin = 8;
            format = R_FORMAT;
            //TODO: Handle SHAMT

            int shamt = atoi(tokens[3]);
            binaryInstruction.word |= shamt;
            
        } else if (strcmp(opcode, "SRL") == 0) {
            opcodeBin = 9;
            format = R_FORMAT;
            //TODO: Handle SHAMT
            int shamt = atoi(tokens[3]);
            binaryInstruction.word |= shamt;
            
            
        } else if (strcmp(opcode, "LW") == 0) {
            opcodeBin = 10;
            format = I_FORMAT;
        } else if (strcmp(opcode, "SW") == 0) {
            opcodeBin = 11;
            format = I_FORMAT;
        } else {
            opcodeBin = -1;
            format = INVALID_FORMAT;
        }
        
        binaryInstruction.word |= opcodeBin  << 28;
        binaryInstruction.format = format;

        int reg1;
        int reg2;
        int reg3;
        int immediateValue;
        switch (format)
        {
            case R_FORMAT:
                reg1 = atoi(tokens[1] + 1);
                reg2 = atoi(tokens[2] + 1);

                reg3 = atoi(tokens[3] + 1);
                if(opcodeBin == 8 || opcodeBin == 9) reg3 = 0;

                //printBinary(binaryInstruction.word);
                binaryInstruction.word |= reg1 << 23;
                //printBinary(binaryInstruction.word);
                binaryInstruction.word |= reg2 << 18;
                //printBinary(binaryInstruction.word);
                binaryInstruction.word |= reg3 << 13;
                
                // printf("Binary Instruction: %d\n", binaryInstruction.word);
                printBinaryR(binaryInstruction.word);
                
            break;
            
            
            case I_FORMAT:

                reg1 = atoi(tokens[1] + 1);
                reg2 = atoi(tokens[2] + 1);

                immediateValue = atoi(tokens[3]);

                //printBinary(binaryInstruction.word);
                binaryInstruction.word |= reg1 << 23;
                //printBinary(binaryInstruction.word);
                binaryInstruction.word |= reg2 << 18;
                //printBinary(binaryInstruction.word);
                binaryInstruction.word |= immediateValue;
                
                // printf("Binary Instruction: %d\n", binaryInstruction.word);
                printBinaryI(binaryInstruction.word);

            break;

            case J_FORMAT:

                binaryInstruction.word |= atoi(tokens[1]);
                printBinaryJ(binaryInstruction.word);

            break;
            
            default:
            break;
        }
        mainMemory[i] = binaryInstruction;
    
}
    
    // printf("Token 1: %s\n", tokens[0]);
    // printf("Token 2: %s\n", tokens[1]);
    // printf("Token 3: %s\n", tokens[2]);
    // printf("Token 4: %s\n", tokens[3]); 

}
//MARK: PIPELINE
//IF, ID, EX, WB even cycles
//OR
//ID, EX, MEM, WB odd cycles
void runPipeline(){
    initRegisters();
    initPipeline();

    int done = 0;

    while (!done) {
        writeBack();
        memory();
        execute();
        decode();
        fetch(programCounter.word);
        if (pipeline.fetchPhaseInst.format != INVALID_FORMAT){
            programCounter.word++;
        }
        if (pipeline.fetchPhaseInst.format == INVALID_FORMAT || pipeline.decodePhaseInst.format == INVALID_FORMAT){
            break;
        }
        
    }
}


/* Reads text file and writes lines to array of strings*/
void readFileToMemory(char* filepath){

    char* textInstructions = readFile(filepath);
    // printf("%s\n", textInstructions);
    lineCount = 0;


    char* token = strtok(textInstructions, "\n");
    while(token != NULL && lineCount < MAX_LINES){
        strncpy(lines[lineCount], token, MAX_INSTRUCTION_TOKENS - 1);
        lines[lineCount][MAX_INSTRUCTION_TOKENS - 1] = '\0';
        lineCount++;
        token = strtok(NULL, "\n");
    }

    strncpy(lines[lineCount], "[END]", MAX_INSTRUCTION_TOKENS - 1);

    //parseTextInstruction();

}



