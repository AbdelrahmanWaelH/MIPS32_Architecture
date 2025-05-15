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
int fetchReady = 1; //0 for fetch ready, 1 for fetch not ready
int cycle = 0;

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
    struct InstructionParts decodedParts;
};

struct InstructionParts {
    int opcode, r1, r2, r3, imm, shamt, address;
    struct Word r1val, r2val, r3val;
    enum InstructionFormat format;
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
void fetch(){
    if (fetchReady && programCounter.word < MAIN_MEMORY_SIZE) {
        pipeline.fetchPhaseInst = mainMemory[programCounter.word];
    } else {
        pipeline.fetchPhaseInst = (struct Word) { .word=0, .format = INVALID_FORMAT};
    }
}

void decode(){
    struct Word instruction = pipeline.fetchPhaseInst;
    struct InstructionParts parts;
    parts.opcode = (instruction.word & 0xF0000000) >> 28;
    parts.r1 = (instruction.word & 0x0F800000) >> 23;
    parts.r2 = (instruction.word & 0x007C0000) >> 18;
    parts.r3 = (instruction.word & 0x0003E000) >> 13;
    parts.shamt = (instruction.word & 0x00001FFF);
    parts.imm = (instruction.word & 0x0003FFFF);
    parts.address = (instruction.word & 0x0FFFFFFF);  
    parts.r1val = registers[parts.r1];
    parts.r2val = registers[parts.r2];
    parts.r3val = registers[parts.r3];
    //hand off data to the next stage
    pipeline.decodePhaseInst = instruction;
    pipeline.decodedParts = parts;
}
void execute(){
    struct InstructionParts parts = pipeline.decodedParts;
    switch(parts.opcode){
        case 0:  parts.r1val.word = parts.r2val.word + parts.r3val.word; break; //ADD R1 R2 R3 should use WB
        case 1:  parts.r1val.word = parts.r2val.word - parts.r3val.word; break; //SUB R1 R2 R3 should use WB
        case 2:  parts.r1val.word = parts.r2val.word * parts.imm; break; //MULI should use WB
        case 3:  parts.r1val.word = parts.r2val.word + parts.imm; break; //ADDI should use WB
        case 4:  if (parts.r1val.word != parts.r2val.word){ programCounter.word = programCounter.word + 1 + parts.imm;} break; //BNE
        case 5:  parts.r1val.word = parts.r2val.word & parts.imm; break; //ANDI should use WB
        case 6:  parts.r1val.word = parts.r2val.word | parts.imm; break; //ORI should use WB
        case 7:  programCounter.word = (programCounter.word & 0xF0000000) | (parts.address & 0x0FFFFFFF); //PC = PC[31:28] + ADDRESS (28b)
        case 8:  parts.r1val.word = parts.r2val.word << parts.shamt; break; //SLL should use WB
        case 9:  parts.r1val.word = parts.r2val.word >> parts.shamt; break; //SRL should use WB
        case 10:  //LW should use the MEM phase
        case 11: parts.address = parts.r1val.word + parts.imm; break; //SW should use the MEM phase, only address is being calculated here
        default: break;
    }
    pipeline.executePhaseInst = pipeline.decodePhaseInst;
    pipeline.decodedParts = parts; //hand off updated parts
}
void memory(){
    struct InstructionParts parts = pipeline.decodedParts;
    switch(parts.opcode){
        case 10: parts.r1val = mainMemory[parts.address]; break; //LW
        case 11: mainMemory[parts.address] = parts.r1val; break; //SW
        default: break;
    }
    pipeline.memoryPhaseInst = pipeline.executePhaseInst;
    pipeline.decodedParts = parts; //hand off updated parts
}
void writeBack(){
    // if (programCounter.word%2 == 0){
    //     pipeline.writebackPhaseInst = pipeline.executePhaseInst;
    // } else {
    //     pipeline.writebackPhaseInst = pipeline.memoryPhaseInst;
    // }
    struct InstructionParts parts = pipeline.decodedParts;
    switch(parts.opcode){
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
        case 10: registers[parts.r1] = parts.r1val; break; //all instructions with opcode 0-10 will use the WB stage
        case 11: break; //11 doesn't really use this stage but we will keep it for lolz 
        default: break;
    }
}
void handleHazards();



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
    pipeline.decodedParts = (struct InstructionParts) {0};
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
    initMemory();

    int done = 0;
    int cycle = 0;
    int fetchWait = 0; //0 for ready, 1 waiting for memory

    while (!done) {
        writeBack();
        memory();
        // execute();
        // decode();
        // fetch(programCounter.word);
        // if (pipeline.fetchPhaseInst.format != INVALID_FORMAT){
        //     programCounter.word++;
        // }
        // if (pipeline.fetchPhaseInst.format == INVALID_FORMAT || pipeline.decodePhaseInst.format == INVALID_FORMAT){
        //     break;
        // }
        if (fetchWait == 0) {
            fetch();
            if (pipeline.fetchPhaseInst.format != INVALID_FORMAT){
                programCounter.word++;
            }
            fetchWait = 1; //next cycle will use MEM phase not IF
        } else {
            fetchWait = 0; //MEM was used this cycle, so next cycle uses IF
        }


        cycle++;
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



