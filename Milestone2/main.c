#include "FileReader.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

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

struct PipelinePhase {
    struct Word instruction;
    int clocksRemaining;
    bool isComplete;
    bool isReady;
};

struct Pipeline {
    struct PipelinePhase fetchPhaseInst;
    struct PipelinePhase decodePhaseInst;
    struct PipelinePhase executePhaseInst;
    struct PipelinePhase memoryPhaseInst;
    struct PipelinePhase writebackPhaseInst;
};



char lines[MAX_LINES][MAX_INSTRUCTION_TOKENS]; //An array to hold the text instructions after reading from file
int lineCount = 0;

struct Word mainMemory[MAIN_MEMORY_SIZE];
struct Word registers[REGISTER_COUNT];
struct Word programCounter;
struct Pipeline pipeline;
int cycle = 1;

char* printOpcodeToName(int instruction){

    if (instruction == 0) return "----";
    int code = instruction >> 28;
    switch (code)
    {
    case 0:
        return ("ADD");
        break;
    case 1:
        return ("SUB");
        break;
    case 2:
        return ("MULI");
        break;
    case 3:
        return ("ADDI");
        break;
    case 4:
        return ("BNE");
        break;
    case 5:
        return ("ANDI");
        break;
    case 6:
        return ("ORI");
        break;
    case 7:
        return ("J");
        break;
    case 8:
        return ("SLL");
        break;
    case 9:
        return ("SRL");
        break;
    case 10:
        return ("LW");
        break;
    case 11:
        return ("SW");
        break;
    default:
        return ("UNKNOWN OPCODE");
        break;
    }
}

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

void printPipeline(){

    printf("F    D    E    M    W\n");
    printf("%s %s %s %s %s\n", 
        printOpcodeToName(pipeline.fetchPhaseInst.instruction.word),
        printOpcodeToName(pipeline.decodePhaseInst.instruction.word),
        printOpcodeToName(pipeline.executePhaseInst.instruction.word),
        printOpcodeToName(pipeline.memoryPhaseInst.instruction.word),
        printOpcodeToName(pipeline.writebackPhaseInst.instruction.word));
    // printf("Remaining:\nF D E M W\n");
    // printf("%d %d %d %d %d\n",
    //     pipeline.fetchPhaseInst.clocksRemaining,
    //     pipeline.decodePhaseInst.clocksRemaining,
    //     pipeline.executePhaseInst.clocksRemaining,
    //     pipeline.memoryPhaseInst.clocksRemaining,
    //     pipeline.writebackPhaseInst.clocksRemaining
    // );

}


void parseTextInstruction(); // Parses the text instructions into their binary representation.
void readFileToMemory(char* filepath);
void initRegisters();
void initMemory();
void initPipeline();
void updatePipeline();
void fetch();
void decode();
void execute();
void memory();
void writeBack();
void handleHazards();

int main(){

    readFileToMemory(filepath);

    initRegisters();
    initPipeline();
    initMemory();

    for(int i = 0; i < MAX_LINES; i++){
        printf("%s\n", lines[i]);
        if(strcmp(lines[i], "[END]") == 0) break;
    }

    parseTextInstruction();

    printMainMemory();
    printf("=====================\n");



    for (int i = 0; i < lineCount; i++){

        updatePipeline();

        printf("==== Cycle %d ====\n", cycle);
        printPipeline();
        cycle++;
        

    }


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

    pipeline.fetchPhaseInst = (struct PipelinePhase) {.instruction = {0}, .clocksRemaining = 0, .isComplete = true, .isReady = true};
    pipeline.decodePhaseInst = (struct PipelinePhase) {.instruction = {0}, .clocksRemaining = 0, .isComplete = true, .isReady = true};
    pipeline.executePhaseInst= (struct PipelinePhase) {.instruction = {0}, .clocksRemaining = 0, .isComplete = true, .isReady = true};
    pipeline.memoryPhaseInst = (struct PipelinePhase) {.instruction = {0}, .clocksRemaining = 0, .isComplete = true, .isReady = true};
    pipeline.writebackPhaseInst = (struct PipelinePhase) {.instruction = {0}, .clocksRemaining = 0, .isComplete = true, .isReady = true};



}

void updatePipeline(){


    

    //Update Writeback

    if(pipeline.writebackPhaseInst.isComplete){

        if(pipeline.memoryPhaseInst.isComplete && pipeline.memoryPhaseInst.instruction.word != 0){

            pipeline.writebackPhaseInst.instruction = pipeline.memoryPhaseInst.instruction;
            pipeline.writebackPhaseInst.clocksRemaining = 1;
            pipeline.writebackPhaseInst.isComplete = false;
            pipeline.memoryPhaseInst.instruction = (struct Word){0};


        } else {

            pipeline.writebackPhaseInst.instruction = (struct Word){0};
            pipeline.writebackPhaseInst.isComplete = true;

        }

    }

    //Update Memory

    if(pipeline.memoryPhaseInst.isComplete){

        if(pipeline.executePhaseInst.isComplete && pipeline.executePhaseInst.instruction.word != 0){

            pipeline.memoryPhaseInst.instruction = pipeline.executePhaseInst.instruction;
            pipeline.memoryPhaseInst.clocksRemaining = 1;
            pipeline.memoryPhaseInst.isComplete = false;
            pipeline.executePhaseInst.instruction = (struct Word){0};


        } else {

            pipeline.memoryPhaseInst.instruction = (struct Word){0};
            pipeline.memoryPhaseInst.isComplete = true;

        }

    }

    //Update Execute

    if(pipeline.executePhaseInst.isComplete){

        if(pipeline.decodePhaseInst.isComplete && pipeline.decodePhaseInst.instruction.word != 0){

            pipeline.executePhaseInst.instruction = pipeline.decodePhaseInst.instruction;
            pipeline.executePhaseInst.clocksRemaining = 2;
            pipeline.executePhaseInst.isComplete = false;
            pipeline.decodePhaseInst.instruction = (struct Word){0};


        } else {

            pipeline.executePhaseInst.instruction = (struct Word){0};
            pipeline.executePhaseInst.isComplete = true;

        }

    }


    //Update Decode

    if(pipeline.decodePhaseInst.isComplete){

        if(pipeline.fetchPhaseInst.isComplete && pipeline.fetchPhaseInst.instruction.word != 0){

            pipeline.decodePhaseInst.instruction = pipeline.fetchPhaseInst.instruction;
            pipeline.decodePhaseInst.clocksRemaining = 2;
            pipeline.decodePhaseInst.isComplete = false;
            pipeline.fetchPhaseInst.instruction = (struct Word){0};

        } else {

            pipeline.decodePhaseInst.instruction = (struct Word){0};
            pipeline.decodePhaseInst.isComplete = true;

        }

    }

    //Update Fetch

    if(cycle % 2 == 1){

        fetch();

    }

    if(pipeline.fetchPhaseInst.clocksRemaining > 0)
    pipeline.fetchPhaseInst.clocksRemaining--;

    if(pipeline.decodePhaseInst.clocksRemaining > 0)
    pipeline.decodePhaseInst.clocksRemaining--;

    if(pipeline.executePhaseInst.clocksRemaining > 0)
    pipeline.executePhaseInst.clocksRemaining--;

    if(pipeline.memoryPhaseInst.clocksRemaining > 0)
    pipeline.memoryPhaseInst.clocksRemaining--;

    if(pipeline.writebackPhaseInst.clocksRemaining > 0)
    pipeline.writebackPhaseInst.clocksRemaining--;

    if(pipeline.fetchPhaseInst.clocksRemaining == 0) {
        pipeline.fetchPhaseInst.isComplete = true;
        pipeline.fetchPhaseInst.clocksRemaining = 0;
    }
    if(pipeline.decodePhaseInst.clocksRemaining == 0) {
        pipeline.decodePhaseInst.isComplete = true;
        pipeline.decodePhaseInst.clocksRemaining = 0;

    }
    if(pipeline.executePhaseInst.clocksRemaining == 0) {
        pipeline.executePhaseInst.isComplete = true;
        pipeline.executePhaseInst.clocksRemaining = 0;
    }
    if(pipeline.memoryPhaseInst.clocksRemaining == 0) {
        pipeline.memoryPhaseInst.isComplete = true;
        pipeline.memoryPhaseInst.clocksRemaining = 0;
    }
    if(pipeline.writebackPhaseInst.clocksRemaining == 0) {
        pipeline.writebackPhaseInst.isComplete = true;
        pipeline.writebackPhaseInst.clocksRemaining = 0;

    }


}

void fetch(){


    pipeline.fetchPhaseInst.instruction = mainMemory[programCounter.word];
    programCounter.word+=1;

    pipeline.fetchPhaseInst.isComplete = false;
    pipeline.fetchPhaseInst.clocksRemaining = 1;

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



