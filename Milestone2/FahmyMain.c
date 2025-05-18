#include "FileReader.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#define MAIN_MEMORY_SIZE 2048
#define WORD_SIZE 32
#define REGISTER_COUNT 32
#define MAX_LINES 64
#define MAX_INSTRUCTION_TOKENS 256

struct DecodedInstructionFields {

    int opcode;
    int r1;
    int r2;
    int r3;
    int shamt;
    int immediate;
    int address;
    int r1val; 
    int r2val;
    int r3val;
};

struct Pipeline {
    struct DecodedInstructionFields decodedInstructionFields;
    int fetchPhaseInst;
    int decodePhaseInst;
    int executePhaseInst;
    int memoryPhaseInst;
    int writebackPhaseInst;
    int decodeCyclesRemaining;
    int executeCyclesRemaining;
};


void parseTextInstruction(); // Parses the text instructions into their binary representation.
void readFileToMemory(char* filepath);
void initRegisters();
void initMemory();
void initPipeline();
void runPipeline();

char lines[MAX_LINES][MAX_INSTRUCTION_TOKENS]; //An array to hold the text instructions after reading from file
int lineCount = 0;

int mainMemory[MAIN_MEMORY_SIZE];
int registers[REGISTER_COUNT];
int programCounter = 0;
struct Pipeline pipeline;
int temporaryExecuteResult = 0;
int temporaryExecuteDestination = 0;
bool temporaryShouldBranch = 0;

char* filepath = "../FahmyCodeTest";
int cycle = 1;
bool fetchReady = true;



void initRegisters(){
    for(int i = 0; i < REGISTER_COUNT; i++)
        registers[i] = 0;
}

void initMemory(){
    for(int i = 0; i < MAIN_MEMORY_SIZE; i++)
        mainMemory[i] = 0;
}

void initPipeline() {
    pipeline.fetchPhaseInst = 0;

    pipeline.decodePhaseInst = 0;
    pipeline.decodedInstructionFields.opcode = 0;
    pipeline.decodedInstructionFields.r1 = 0;
    pipeline.decodedInstructionFields.r2 = 0;
    pipeline.decodedInstructionFields.r3 = 0;
    pipeline.decodedInstructionFields.shamt = 0;
    pipeline.decodedInstructionFields.immediate = 0;
    pipeline.decodedInstructionFields.address = 0;

    pipeline.executePhaseInst = 0;
    pipeline.memoryPhaseInst = 0;
    pipeline.writebackPhaseInst = 0;

    pipeline.decodeCyclesRemaining = 0;
    pipeline.executeCyclesRemaining = 0;

}



void fetch();
void decode();
void execute();
void memory();
void writeback();
void runPipeline();

void printMainMemory();
void printPipeline();
void printRegisters();
void printRegistersMinimal();

bool pipelineDone() {
    return pipeline.fetchPhaseInst == 0 &&
        pipeline.decodePhaseInst == 0 &&
        pipeline.executePhaseInst == 0 &&
        pipeline.memoryPhaseInst == 0 &&
        pipeline.writebackPhaseInst == 0;
}

int main() {
    readFileToMemory(filepath);
    parseTextInstruction();


    runPipeline();
    cycle++;
    while (!pipelineDone()) {
        runPipeline();
        cycle++;
        if (cycle == 100) break;
    }

    printRegisters();

}

void runPipeline() {

    printf("--- Cycle %d ---", cycle);
    //
    writeback();
    memory();
    execute();
    decode();
    fetch();

    printPipeline();
    printRegistersMinimal();

}

void fetch() {
    if (fetchReady) {
        pipeline.fetchPhaseInst = mainMemory[programCounter];
        programCounter++;
        fetchReady = false;
    }else {
        pipeline.fetchPhaseInst = 0;
        fetchReady = true;
    }
}

void decode() {

    if (pipeline.fetchPhaseInst == 0 && pipeline.decodePhaseInst == 0) return;

    if (pipeline.decodeCyclesRemaining == 0)
        pipeline.decodePhaseInst = pipeline.fetchPhaseInst;
        if (pipeline.decodePhaseInst == 0) return;


    if (pipeline.decodeCyclesRemaining == 0) {

        pipeline.decodeCyclesRemaining = 1;
    }else {
        pipeline.decodedInstructionFields.opcode     = (pipeline.decodePhaseInst >> 28) & 0xF;
        pipeline.decodedInstructionFields.r1         = (pipeline.decodePhaseInst >> 23) & 0x1F;
        pipeline.decodedInstructionFields.r2         = (pipeline.decodePhaseInst >> 18) & 0x1F;
        pipeline.decodedInstructionFields.r3         = (pipeline.decodePhaseInst >> 13) & 0x1F;
        pipeline.decodedInstructionFields.shamt      = pipeline.decodePhaseInst & 0x1FFF;
        pipeline.decodedInstructionFields.immediate  = pipeline.decodePhaseInst & 0x3FFFF;
        if ((pipeline.decodedInstructionFields.immediate & 0x20000) >> 17 == 1)
            pipeline.decodedInstructionFields.immediate |= 0xFFFC0000; // Make it negative
        pipeline.decodedInstructionFields.address    = pipeline.decodePhaseInst & 0xFFFFFFF;
        pipeline.decodedInstructionFields.r1val = registers[pipeline.decodedInstructionFields.r1];
        pipeline.decodedInstructionFields.r2val = registers[pipeline.decodedInstructionFields.r2];
        pipeline.decodedInstructionFields.r3val = registers[pipeline.decodedInstructionFields.r3];
        pipeline.decodeCyclesRemaining--;
    }

}

void execute() {

    if (pipeline.executePhaseInst == 0) pipeline.executeCyclesRemaining = 0;

    if (pipeline.decodePhaseInst == 0 && pipeline.executePhaseInst == 0) return;

    if (pipeline.executePhaseInst == 0 && pipeline.decodeCyclesRemaining != 0) return;

    if ( pipeline.executeCyclesRemaining == 0 && pipeline.decodeCyclesRemaining == 0)
        pipeline.executePhaseInst = pipeline.decodePhaseInst;

    if (pipeline.executeCyclesRemaining == 0) {

        pipeline.executeCyclesRemaining = 1;

    }else {
        switch (pipeline.decodedInstructionFields.opcode) {
            case 0: //ADD
                temporaryExecuteResult = pipeline.decodedInstructionFields.r2val + pipeline.decodedInstructionFields.r3val;
                temporaryExecuteDestination = pipeline.decodedInstructionFields.r1;
                break;
            case 1: //SUB
                temporaryExecuteResult = pipeline.decodedInstructionFields.r2val - pipeline.decodedInstructionFields.r3val;
                temporaryExecuteDestination = pipeline.decodedInstructionFields.r1;
                break;
            case 2: //MULI
                temporaryExecuteResult = pipeline.decodedInstructionFields.r2val * pipeline.decodedInstructionFields.immediate;
                temporaryExecuteDestination = pipeline.decodedInstructionFields.r1;
                break;
            case 3: //ADDI
                temporaryExecuteResult = pipeline.decodedInstructionFields.r2val + pipeline.decodedInstructionFields.immediate;
                temporaryExecuteDestination = pipeline.decodedInstructionFields.r1;
                break;
            case 4: //BNE
                temporaryExecuteResult = programCounter + 1 + pipeline.decodedInstructionFields.immediate;
                temporaryExecuteDestination = programCounter;
                temporaryShouldBranch =
                pipeline.decodedInstructionFields.r1val != pipeline.decodedInstructionFields.r2val ? true : false;
                break;
            case 5: //ANDI
                temporaryExecuteResult = pipeline.decodedInstructionFields.r2val & pipeline.decodedInstructionFields.immediate;
                temporaryExecuteDestination = pipeline.decodedInstructionFields.r1;
                break;
            case 6: //ORI
                temporaryExecuteResult = pipeline.decodedInstructionFields.r2val | pipeline.decodedInstructionFields.immediate;
                temporaryExecuteDestination = pipeline.decodedInstructionFields.r1;
                break;
            case 7: //J
                temporaryExecuteResult = (programCounter & 0xF0000000) | pipeline.decodedInstructionFields.address;
                temporaryExecuteDestination = pipeline.decodedInstructionFields.r1;
                break;
            case 8: //SLL
                temporaryExecuteResult = pipeline.decodedInstructionFields.r2 << pipeline.decodedInstructionFields.shamt;
                temporaryExecuteDestination = pipeline.decodedInstructionFields.r1;
                break;
            case 9: //SRL
                temporaryExecuteResult = pipeline.decodedInstructionFields.r2 >> pipeline.decodedInstructionFields.shamt;
                temporaryExecuteDestination = pipeline.decodedInstructionFields.r1;
                break;
            case 10: //LW
                temporaryExecuteResult = pipeline.decodedInstructionFields.r2val + pipeline.decodedInstructionFields.immediate;
                //Must pass through memory phase
                temporaryExecuteDestination = pipeline.decodedInstructionFields.r1;
            case 11: //SW
                temporaryExecuteResult = pipeline.decodedInstructionFields.r2val + pipeline.decodedInstructionFields.immediate;
                //Must pass through memory phase
                temporaryExecuteDestination = pipeline.decodedInstructionFields.r1;
            default:
                break;
        }

        pipeline.executeCyclesRemaining--;
    }

}

void memory() {
    if (pipeline.memoryPhaseInst == 0 && pipeline.executePhaseInst == 0) return;
    if (pipeline.executeCyclesRemaining != 0 && pipeline.memoryPhaseInst == 0) return;


    if (pipeline.executeCyclesRemaining == 0) {
        pipeline.memoryPhaseInst = pipeline.executePhaseInst;

        //We don't use decoded parts because next instruction is decoded and we lose the values of current instruction
        if (((pipeline.memoryPhaseInst >> 28) & 0xF) == 10)
            temporaryExecuteResult = mainMemory[temporaryExecuteResult];
        if (((pipeline.memoryPhaseInst >> 28) & 0xF) == 11)
            mainMemory[temporaryExecuteResult] = registers[temporaryExecuteDestination]; //not entirely correct, performs WB in memory stage
    }else {
        pipeline.memoryPhaseInst = 0;
    }


}


void writeback() {

    if (pipeline.writebackPhaseInst == 0 && pipeline.memoryPhaseInst == 0) return;

    if (pipeline.memoryPhaseInst != 0) {
        pipeline.writebackPhaseInst = pipeline.memoryPhaseInst;

        if (((pipeline.writebackPhaseInst >> 28 ) & 0xF) != 10 && ((pipeline.writebackPhaseInst >> 28) & 0xF ) != 11 &&
            ((pipeline.writebackPhaseInst >> 28 ) & 0xF )!= 7 && ((pipeline.writebackPhaseInst >> 28 ) & 0xF ) != 4) {
            registers[temporaryExecuteDestination] = temporaryExecuteResult;
        }else if (((pipeline.writebackPhaseInst >> 28) & 0xF ) == 7) {
                programCounter = temporaryExecuteResult;
        }else if (((pipeline.writebackPhaseInst >> 28) & 0xF ) == 4) {

            if (temporaryShouldBranch) {
                programCounter = temporaryExecuteResult;
            }

        } else if (((pipeline.memoryPhaseInst >> 28) & 0xF) == 10){
            registers[temporaryExecuteDestination] = temporaryExecuteResult;
        } else {
            pipeline.writebackPhaseInst = 0;
        }

    }else {
        pipeline.writebackPhaseInst = 0;

    }
    registers[0] = 0;
}

/* Parsing and Loading Methods */

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
        int binaryInstruction = 0;
        int opcodeBin = 0;

        if (strcmp(opcode, "ADD") == 0) {
            opcodeBin = 0;
        } else if (strcmp(opcode, "SUB") == 0) {
            opcodeBin = 1;
        } else if (strcmp(opcode, "MULI") == 0) {
            opcodeBin = 2;
        } else if (strcmp(opcode, "ADDI") == 0) {
            opcodeBin = 3;
        } else if (strcmp(opcode, "BNE") == 0) {
            opcodeBin = 4;
        } else if (strcmp(opcode, "ANDI") == 0) {
            opcodeBin = 5;
        } else if (strcmp(opcode, "ORI") == 0) {
            opcodeBin = 6;
        } else if (strcmp(opcode, "J") == 0) {
            opcodeBin = 7;
        } else if (strcmp(opcode, "SLL") == 0) {
            opcodeBin = 8;
            //TODO: Handle SHAMT

            int shamt = atoi(tokens[3]);
            binaryInstruction |= shamt;

        } else if (strcmp(opcode, "SRL") == 0) {
            opcodeBin = 9;
            //TODO: Handle SHAMT
            int shamt = atoi(tokens[3]);
            binaryInstruction |= shamt;


        } else if (strcmp(opcode, "LW") == 0) {
            opcodeBin = 10;
        } else if (strcmp(opcode, "SW") == 0) {
            opcodeBin = 11;
        } else {
            opcodeBin = -1;
        }

        binaryInstruction |= opcodeBin  << 28;

        int reg1;
        int reg2;
        int reg3;
        int immediateValue;

        if (opcodeBin == 0 || opcodeBin == 1 || opcodeBin == 8 || opcodeBin == 9) {
            reg1 = atoi(tokens[1] + 1);
            reg2 = atoi(tokens[2] + 1);

            reg3 = atoi(tokens[3] + 1);
            if(opcodeBin == 8 || opcodeBin == 9) reg3 = 0;

            binaryInstruction |= reg1 << 23;
            binaryInstruction |= reg2 << 18;
            binaryInstruction |= reg3 << 13;

        } else if (opcodeBin == 7) {
            binaryInstruction |= atoi(tokens[1]);
        } else {
            reg1 = atoi(tokens[1] + 1);
            reg2 = atoi(tokens[2] + 1);
            immediateValue = atoi(tokens[3]);

            binaryInstruction |= reg1 << 23;
            binaryInstruction |= reg2 << 18;
            binaryInstruction |= immediateValue & 0x3FFFF;

        }
        mainMemory[i] = binaryInstruction;
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
        lines[lineCount][MAX_INSTRUCTION_TOKENS - 1] = '\0';  // Null-terminate to be safe

        // Remove trailing \r if it exists
        size_t len = strlen(lines[lineCount]);
        if (len > 0 && lines[lineCount][len - 1] == '\r') {
            lines[lineCount][len - 1] = '\0';
        }

        lines[lineCount][MAX_INSTRUCTION_TOKENS - 1] = '\0';
        lineCount++;
        token = strtok(NULL, "\n");
    }

    strncpy(lines[lineCount], "[END]", MAX_INSTRUCTION_TOKENS - 1);


}

void printRInstruction(int instruction);
void printJInstruction(int instruction);
void printIInstruction(int instruction);
char* getInstructionText(int instruction);

void printMainMemory() {

    for (int i = 0; i < MAIN_MEMORY_SIZE; i++) {
        int opcode = mainMemory[i] >> 28;
        if (opcode == 0 || opcode == 1 || opcode == 8 || opcode == 9) {
            printRInstruction(mainMemory[i]);
        } else if (opcode == 7) {
            printJInstruction(mainMemory[i]);
        }else {
            printIInstruction(mainMemory[i]);
        }
    }

}

void printRegisters() {

    printf("Registers:\n");
    for (int i =0; i < REGISTER_COUNT; i++) {
        printf("R%d: %d: ", i, registers[i]);
        if ((i+1) % 4 != 0) printf(" ");
        else printf("\n");
    }

}

void printRegistersMinimal() {

    printf("\n");
    for (int i =0; i < 8; i++) {
        printf("R%d: %d: ", i, registers[i]);
        printf(" ");
    }
    printf("\n");

}

void printPipeline() {
    printf("  PC: %d\n", programCounter);
    printf("  IF:  %s\n", getInstructionText(pipeline.fetchPhaseInst));
    printf("  ID:  %s\n", getInstructionText(pipeline.decodePhaseInst));
    printf("  EX:  %s\n", getInstructionText(pipeline.executePhaseInst));
    printf("  MEM: %s\n", getInstructionText(pipeline.memoryPhaseInst));
    printf("  WB:  %s\n", getInstructionText(pipeline.writebackPhaseInst));
}

void printRInstruction(int instruction) {
    // Extract fields
    int opcode     = (instruction >> 28) & 0xF;
    int r1         = (instruction >> 23) & 0x1F;
    int r2         = (instruction >> 18) & 0x1F;
    int r3         = (instruction >> 13) & 0x1F;
    int shamt      = instruction & 0x1FFF;
    int immediate  = instruction & 0x3FFFF;
    int address    = instruction & 0xFFFFFFF;

    printf("%d %d %d %d %d\n", opcode, r1, r2, r3, shamt);

}

void printJInstruction(int instruction) {
    int opcode     = (instruction >> 28) & 0xF;
    int r1         = (instruction >> 23) & 0x1F;
    int r2         = (instruction >> 18) & 0x1F;
    int r3         = (instruction >> 13) & 0x1F;
    int shamt      = instruction & 0x1FFF;
    int immediate  = instruction & 0x3FFFF;
    int address    = instruction & 0xFFFFFFF;

    printf("%d %d \n", opcode, address);
}

void printIInstruction(int instruction) {
    int opcode     = (instruction >> 28) & 0xF;
    int r1         = (instruction >> 23) & 0x1F;
    int r2         = (instruction >> 18) & 0x1F;
    int r3         = (instruction >> 13) & 0x1F;
    int shamt      = instruction & 0x1FFF;
    int immediate  = instruction & 0x3FFFF;
    int address    = instruction & 0xFFFFFFF;

    printf("%d %d %d %d \n", opcode, r1, r2, immediate);
}

char* getInstructionText(int instruction) {
    static char instructionText[50]; // Static buffer to hold the instruction text

    if (instruction == 0) {
        strcpy(instructionText, "-");
        return instructionText;
    }

    int opcode = ((instruction & 0xF0000000) >> 28) & 0xF;
    int r1 = (instruction & 0x0F800000) >> 23;
    int r2 = (instruction & 0x007C0000) >> 18;
    int r3 = (instruction & 0x0003E000) >> 13;
    int shamt = (instruction & 0x00001FFF);
    int imm = (instruction & 0x0003FFFF);
    if ((imm & 0x20000) >> 17 == 1)
        imm |= 0xFFFC0000; // Make it negative
    int address = (instruction & 0x0FFFFFFF);

    switch(opcode) {
        case 0:  // ADD
            sprintf(instructionText, "ADD R%d R%d R%d", r1, r2, r3);
            break;
        case 1:  // SUB
            sprintf(instructionText, "SUB R%d R%d R%d", r1, r2, r3);
            break;
        case 2:  // MULI
            sprintf(instructionText, "MULI R%d R%d %d", r1, r2, imm);
            break;
        case 3:  // ADDI
            sprintf(instructionText, "ADDI R%d R%d %d", r1, r2, imm);
            break;
        case 4:  // BNE
            sprintf(instructionText, "BNE R%d R%d %d", r1, r2, imm);
            break;
        case 5:  // ANDI
            sprintf(instructionText, "ANDI R%d R%d %d", r1, r2, imm);
            break;
        case 6:  // ORI
            sprintf(instructionText, "ORI R%d R%d %d", r1, r2, imm);
            break;
        case 7:  // J
            sprintf(instructionText, "J %d", address);
            break;
        case 8:  // SLL
            sprintf(instructionText, "SLL R%d R%d %d", r1, r2, shamt);
            break;
        case 9:  // SRL
            sprintf(instructionText, "SRL R%d R%d %d", r1, r2, shamt);
            break;
        case 10: // LW
            sprintf(instructionText, "LW R%d R%d %d", r1, r2, imm);
            break;
        case 11: // SW
            sprintf(instructionText, "SW R%d R%d %d", r1, r2, imm);
            break;
        default:
            sprintf(instructionText, "UNKNOWN");
            break;
    }

    return instructionText;
}