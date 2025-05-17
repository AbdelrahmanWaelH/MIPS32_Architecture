#include "FileReader.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAIN_MEMORY_SIZE 2048
#define WORD_SIZE 32
#define REGISTER_COUNT 32
#define MAX_LINES 64
#define MAX_INSTRUCTION_TOKENS 256

// Modify function declarations to use pointers
void parseTextInstruction(); // Parses the text instructions into their binary representation.
void readFileToMemory(char* filepath);
void initRegisters();
void initMemory();
void initPipeline();
void runPipeline();

char* filepath = "test_combined_hazards.txt";
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

struct InstructionParts {
    int opcode, r1, r2, r3, imm, shamt, address;
    struct Word r1val, r2val, r3val;
    enum InstructionFormat format;
    int writeBackRegister1;
    struct Word writeBackValue1;
    int writeBackRegister2;
    struct Word writeBackValue2;
};

struct Pipeline {
    struct Word fetchPhaseInst;
    struct Word decodePhaseInst;
    struct Word executePhaseInst;
    struct Word memoryPhaseInst;
    struct Word writebackPhaseInst;
    struct InstructionParts decodedParts;
};

char lines[MAX_LINES][MAX_INSTRUCTION_TOKENS]; //An array to hold the text instructions after reading from file
int lineCount = 0;

struct Word mainMemory[MAIN_MEMORY_SIZE];
struct Word registers[REGISTER_COUNT];
struct Word programCounter;
struct Pipeline pipeline;
// Enhance the ForwardingUnit struct to better track data hazards
struct ForwardingUnit {
    int forwardA;       // 0: no forwarding, 1: from EX/MEM, 2: from MEM/WB
    int forwardB;       // 0: no forwarding, 1: from EX/MEM, 2: from MEM/WB
    int sourceRegA;     // Register read in decode stage (rs)
    int sourceRegB;     // Register read in decode stage (rt)
    struct Word forwardValueA; // Value to be forwarded for rs
    struct Word forwardValueB; // Value to be forwarded for rt
    int stall;          // 0: no stall, 1: stall needed
};

// Enhanced branch predictor with 2-bit saturating counter
struct BranchPredictor {
    int branchTaken;    // 0: not taken (prediction), 1: taken
    int branchResolved; // 0: not resolved, 1: resolved
    int targetAddress;  // Branch target address if taken
    int flushPipeline;  // 0: no flush, 1: flush required
    int predictionBits; // 2-bit saturating counter: 00-strongly not taken, 01-weakly not taken, 10-weakly taken, 11-strongly taken
    int lastBranchResult; // Result of last branch: 0-not taken, 1-taken
};

// Add structure for structural hazard detection
struct StructuralHazard {
    int memoryBusy;     // 0: memory bus available, 1: memory bus busy
    int stall;          // 0: no stall, 1: stall needed due to structural hazard
};

struct ForwardingUnit forwardingUnit;
struct BranchPredictor branchPredictor;
struct StructuralHazard structuralHazard;

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

// Update function implementations to use pointers
void fetch(){
    if (fetchReady && programCounter.word < MAIN_MEMORY_SIZE) {
        pipeline.fetchPhaseInst = mainMemory[programCounter.word];
    } else {
        pipeline.fetchPhaseInst = (struct Word) { .word=0, .format = INVALID_FORMAT};
    }
    //the logic here is that it either hands off the right data when it's time, or nothing at all when we're in an _odd_ stage
}

void decode(){
    struct Word instruction = pipeline.fetchPhaseInst;
    struct InstructionParts *parts = &pipeline.decodedParts;
    parts->opcode = (instruction.word & 0xF0000000) >> 28;
    parts->r1 = (instruction.word & 0x0F800000) >> 23;
    parts->r2 = (instruction.word & 0x007C0000) >> 18;
    parts->r3 = (instruction.word & 0x0003E000) >> 13;
    parts->shamt = (instruction.word & 0x00001FFF);
    parts->imm = (instruction.word & 0x0003FFFF);
    parts->address = (instruction.word & 0x000007FF);
    parts->r1val = registers[parts->r1];
    parts->r2val = registers[parts->r2];
    parts->r3val = registers[parts->r3];
    //hand off data to the next stage
    pipeline.decodePhaseInst = instruction;
}

void execute(){
    struct InstructionParts *parts = &pipeline.decodedParts;
    switch(parts->opcode){
        case 0:  parts->r1val.word = parts->r2val.word + parts->r3val.word; break; //ADD R1 R2 R3 should use WB
        case 1:  parts->r1val.word = parts->r2val.word - parts->r3val.word; break; //SUB R1 R2 R3 should use WB
        case 2:  parts->r1val.word = parts->r2val.word * parts->imm; break; //MULI should use WB
        case 3:  parts->r1val.word = parts->r2val.word + parts->imm; break; //ADDI should use WB
        case 4:  if (parts->r1val.word != parts->r2val.word){ parts->address = programCounter.word + 1 + parts->imm;} break; //BNE
        case 5:  parts->r1val.word = parts->r2val.word & parts->imm; break; //ANDI should use WB
        case 6:  parts->r1val.word = parts->r2val.word | parts->imm; break; //ORI should use WB
        case 7:  programCounter.word = (programCounter.word & 0xF0000000) | (parts->address & 0x0FFFFFFF); //PC = PC[31:28] + ADDRESS (28b)
        case 8:  parts->r1val.word = parts->r2val.word << parts->shamt; break; //SLL should use WB
        case 9:  parts->r1val.word = parts->r2val.word >> parts->shamt; break; //SRL should use WB
        case 10:  //LW should use the MEM phase
        case 11: parts->address = parts->r1val.word + parts->imm; break; //SW should use the MEM phase, only address is being computed here
        default: break;
    }
    parts->writeBackRegister2 = parts->writeBackRegister1;
    parts->writeBackValue2 = parts->writeBackValue1;
    int opcode = parts->opcode;
    if (opcode!=4 && opcode!=7 && opcode!=11) {
        parts->writeBackRegister1 = parts->r1;
        parts->writeBackValue1 = parts->r1val;
    }
    pipeline.executePhaseInst = pipeline.decodePhaseInst;
}

void memory(){
    struct InstructionParts *parts = &pipeline.decodedParts;
    switch(parts->opcode){
        case 10: parts->r1val = mainMemory[parts->address]; break; //LW
        case 11: mainMemory[parts->address] = parts->r1val; break; //SW
        default: break;
    } //performs the actual data transfer to memory
    pipeline.memoryPhaseInst = pipeline.executePhaseInst;
}

void writeBack(){
    struct InstructionParts *parts = &pipeline.decodedParts;
    switch(parts->opcode){
        case 0: case 1: case 2: case 3: case 5: case 6: case 8: case 9: case 10: 
            if (parts->writeBackRegister2 != 0) {registers[parts->writeBackRegister2] = parts->writeBackValue2;} break; //all instructions with opcode 0-10 will use the WB stage
        //prevents writing of the zero register
        case 4: programCounter.word = parts->address;//BNE, TODO store ADDRESS in programCounter
        case 7: //JMP
        case 11: break; //11 doesn't really use this stage but we will keep it for lolz 
        default: break;
    }
    if (cycle%2 == 0){
        pipeline.writebackPhaseInst = pipeline.executePhaseInst;
    } else {
        pipeline.writebackPhaseInst = pipeline.memoryPhaseInst;
    }
}

void handleHazards();



int main(){
    runPipeline();
}

// ... existing code ...


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
    
    // Initialize hazard handling units with enhanced fields
    forwardingUnit.forwardA = 0;
    forwardingUnit.forwardB = 0;
    forwardingUnit.sourceRegA = 0;
    forwardingUnit.sourceRegB = 0;
    forwardingUnit.forwardValueA.word = 0;
    forwardingUnit.forwardValueB.word = 0;
    forwardingUnit.stall = 0;
    
    branchPredictor.branchTaken = 0;
    branchPredictor.branchResolved = 0;
    branchPredictor.targetAddress = 0;
    branchPredictor.flushPipeline = 0;
    branchPredictor.predictionBits = 0; // Start with strongly not taken
    branchPredictor.lastBranchResult = 0;
    
    structuralHazard.memoryBusy = 0;
    structuralHazard.stall = 0;
    
    programCounter.word = 0; // Initialize PC
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

    //parseTextInstruction();

}

// Enhanced data hazard detection with load-use stall detection
void detectDataHazards() {
    // Reset forwarding flags and stall
    forwardingUnit.forwardA = 0;
    forwardingUnit.forwardB = 0;
    forwardingUnit.stall = 0;
    
    // Get source registers from decode stage
    forwardingUnit.sourceRegA = pipeline.decodedParts.r2; // rs
    forwardingUnit.sourceRegB = pipeline.decodedParts.r3; // rt
    
    // Only check if we have a valid instruction in decode
    if (pipeline.decodePhaseInst.format != INVALID_FORMAT) {
        int decodeOpcode = (pipeline.decodePhaseInst.word & 0xF0000000) >> 28;
        
        // Check for load-use data hazard (LW in EX followed by instruction using that data in ID)
        if (pipeline.executePhaseInst.format != INVALID_FORMAT) {
            int exOpcode = (pipeline.executePhaseInst.word & 0xF0000000) >> 28;
            int exDestReg = (pipeline.executePhaseInst.word & 0x0F800000) >> 23; // r1 (destination)
            
            // If LW in EX and the next instruction needs that data, must stall
            if (exOpcode == 10 && exDestReg != 0) { // LW instruction
                if (exDestReg == forwardingUnit.sourceRegA || exDestReg == forwardingUnit.sourceRegB) {
                    printf("  Detected load-use hazard! Stalling...\n");
                    forwardingUnit.stall = 1;
                    return; // Need to stall, don't check other forwarding
                }
            }
            
            // Handle other data hazards with forwarding
            if (exDestReg != 0 && (exOpcode != 11 && exOpcode != 4 && exOpcode != 7)) {
                // Regular forwarding from EX stage
                if (exDestReg == forwardingUnit.sourceRegA) {
                    printf("  Forwarding EX result to rs (R%d)\n", forwardingUnit.sourceRegA);
                    forwardingUnit.forwardA = 1;
                    forwardingUnit.forwardValueA = pipeline.decodedParts.r1val;
                }
                if (exDestReg == forwardingUnit.sourceRegB) {
                    printf("  Forwarding EX result to rt (R%d)\n", forwardingUnit.sourceRegB);
                    forwardingUnit.forwardB = 1;
                    forwardingUnit.forwardValueB = pipeline.decodedParts.r1val;
                }
            }
        }
        
        // Check MEM/WB stage for forwarding
        if (pipeline.memoryPhaseInst.format != INVALID_FORMAT) {
            int memOpcode = (pipeline.memoryPhaseInst.word & 0xF0000000) >> 28;
            int memDestReg = (pipeline.memoryPhaseInst.word & 0x0F800000) >> 23;
            
            if (memDestReg != 0 && (memOpcode != 11 && memOpcode != 4 && memOpcode != 7)) {
                // If EX/MEM isn't already forwarding, check if MEM/WB should
                if (forwardingUnit.forwardA == 0 && memDestReg == forwardingUnit.sourceRegA) {
                    printf("  Forwarding MEM result to rs (R%d)\n", forwardingUnit.sourceRegA);
                    forwardingUnit.forwardA = 2;
                    forwardingUnit.forwardValueA = pipeline.decodedParts.r1val;
                }
                if (forwardingUnit.forwardB == 0 && memDestReg == forwardingUnit.sourceRegB) {
                    printf("  Forwarding MEM result to rt (R%d)\n", forwardingUnit.sourceRegB);
                    forwardingUnit.forwardB = 2;
                    forwardingUnit.forwardValueB = pipeline.decodedParts.r1val;
                }
            }
        }
    }
}

// Enhanced branch prediction with 2-bit predictor
void handleControlHazards() {
    // Check if current instruction in execute stage is a branch
    if (pipeline.executePhaseInst.format != INVALID_FORMAT) {
        int opcode = (pipeline.executePhaseInst.word & 0xF0000000) >> 28;
        
        // Branch instruction (BNE)
        if (opcode == 4) {
            struct InstructionParts *parts = &pipeline.decodedParts;
            int branchCondition = (parts->r1val.word != parts->r2val.word);
            
            // Compare actual branch outcome with prediction
            if (branchCondition != branchPredictor.branchTaken) {
                // Prediction was wrong, need to flush
                branchPredictor.flushPipeline = 1;
                printf("  Branch misprediction detected! Flushing pipeline...\n");
                
                if (branchCondition) {
                    // Branch was taken, but we predicted not taken
                    branchPredictor.targetAddress = programCounter.word + parts->imm;
                } else {
                    // Branch was not taken, but we predicted taken
                    branchPredictor.targetAddress = programCounter.word;
                }
            }
            
            // Update 2-bit saturating counter based on actual branch outcome
            if (branchCondition) { // Branch taken
                if (branchPredictor.predictionBits < 3) {
                    branchPredictor.predictionBits++;
                }
            } else { // Branch not taken
                if (branchPredictor.predictionBits > 0) {
                    branchPredictor.predictionBits--;
                }
            }
            
            // Save result for next prediction
            branchPredictor.lastBranchResult = branchCondition;
            branchPredictor.branchResolved = 1;
            
            // Make prediction for next time this branch is encountered
            branchPredictor.branchTaken = (branchPredictor.predictionBits >= 2);
            
            printf("  Branch predictor updated: %s (counter=%d)\n", 
                   branchPredictor.branchTaken ? "Predict Taken" : "Predict Not Taken",
                   branchPredictor.predictionBits);
        }
        // Jump instruction
        else if (opcode == 7) {
            branchPredictor.branchTaken = 1;
            branchPredictor.targetAddress = (programCounter.word & 0xF0000000) | 
                ((pipeline.executePhaseInst.word & 0x0FFFFFFF)); // Jump target
            branchPredictor.flushPipeline = 1; // Signal pipeline flush
            printf("  Jump detected! Flushing pipeline...\n");
            branchPredictor.branchResolved = 1;
        }
    }
}

void detectStructuralHazards() {
    structuralHazard.stall = 0;
    
    // Check if fetch and memory access would conflict (simplified model)
    if (pipeline.executePhaseInst.format != INVALID_FORMAT) {
        int opcode = (pipeline.executePhaseInst.word & 0xF0000000) >> 28;
        
        // If instruction in EX is LW or SW and would use memory next cycle
        // And fetch would also use memory next cycle
        if ((opcode == 10 || opcode == 11) && fetchReady == 0) {
            printf("  Structural hazard detected: memory conflict! Stalling fetch...\n");
            structuralHazard.memoryBusy = 1;
            structuralHazard.stall = 1;
        } else {
            structuralHazard.memoryBusy = 0;
        }
    }
}

void applyForwarding() {
    // Apply forwarding for rs (r2)
    if (forwardingUnit.forwardA == 1) {
        pipeline.decodedParts.r2val = forwardingUnit.forwardValueA;
    } else if (forwardingUnit.forwardA == 2) {
        pipeline.decodedParts.r2val = forwardingUnit.forwardValueA;
    }
    
    // Apply forwarding for rt (r3)
    if (forwardingUnit.forwardB == 1) {
        pipeline.decodedParts.r3val = forwardingUnit.forwardValueB;
    } else if (forwardingUnit.forwardB == 2) {
        pipeline.decodedParts.r3val = forwardingUnit.forwardValueB;
    }
}

void flushPipeline() {
    // Clear fetch and decode stages
    pipeline.fetchPhaseInst = (struct Word) { .word=0, .format = INVALID_FORMAT };
    pipeline.decodePhaseInst = (struct Word) { .word=0, .format = INVALID_FORMAT };
    
    // Update PC to branch target
    programCounter.word = branchPredictor.targetAddress;
    
    // Reset branch predictor state
    branchPredictor.flushPipeline = 0;
}

// Modified handleHazards to integrate all hazard types
void handleHazards() {
    // First detect structural hazards
    detectStructuralHazards();
    
    // Then detect data hazards if no structural stall
    if (!structuralHazard.stall) {
        detectDataHazards();
    }
    
    // Apply data forwarding if needed and no stall
    if (!forwardingUnit.stall) {
        applyForwarding();
    }
    
    // Handle control hazards
    handleControlHazards();
    
    // If branch taken and pipeline flush needed
    if (branchPredictor.flushPipeline) {
        flushPipeline();
    }
}

char* getInstructionText(struct Word instruction) {
    static char instructionText[50]; // Static buffer to hold the instruction text

    if (instruction.format == INVALID_FORMAT || instruction.word == 0) {
        strcpy(instructionText, "-");
        return instructionText;
    }

    int opcode = (instruction.word & 0xF0000000) >> 28;
    int r1 = (instruction.word & 0x0F800000) >> 23;
    int r2 = (instruction.word & 0x007C0000) >> 18;
    int r3 = (instruction.word & 0x0003E000) >> 13;
    int shamt = (instruction.word & 0x00001FFF);
    int imm = (instruction.word & 0x0003FFFF);
    int address = (instruction.word & 0x0FFFFFFF);

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

// Enhanced runPipeline function with hazard handling
void runPipeline() {
    initRegisters();
    initPipeline();
    initMemory();
    readFileToMemory(filepath);
    parseTextInstruction();
    
    int done = 0;
    int fetchWait = 0; //0 for ready, 1 waiting for memory

    printf("\nStarting pipeline execution for %s...\n", filepath);

    while (!done) {
        printf("\n--- Cycle %d ---\n", cycle);
        
        // Process pipeline stages in reverse order to prevent overwriting
        writeBack();
        
        if (!structuralHazard.stall) {
            memory();
        }
        
        execute();
        
        // Handle all types of hazards
        handleHazards();
        
        // Only proceed with decode if no data hazard stall
        if (!forwardingUnit.stall) {
            decode();
            
            // Only fetch if no structural hazard and it's fetch's turn
            if (!structuralHazard.stall && fetchWait == 0) {
                fetch();
                if (pipeline.fetchPhaseInst.format != INVALID_FORMAT) {
                    // Only increment PC if we're not flushing the pipeline
                    if (!branchPredictor.flushPipeline) {
                        programCounter.word++;
                    }
                } else {
                    // No more instructions to fetch, we might be done
                    if (pipeline.decodePhaseInst.format == INVALID_FORMAT &&
                        pipeline.executePhaseInst.format == INVALID_FORMAT &&
                        pipeline.memoryPhaseInst.format == INVALID_FORMAT) {
                        done = 1; // Pipeline is empty, we're done
                    }
                }
                fetchWait = 1; //next cycle will use MEM phase not IF
            } else {
                fetchWait = 0; //MEM was used this cycle, so next cycle uses IF
            }
        } else {
            printf("  Stalling pipeline due to data hazard...\n");
            // When stalling, we don't advance the pipeline stages before EX
        }
        
        // Print pipeline state for debugging
        printf("  PC: %d\n", programCounter.word);
        printf("  IF:  %s\n", getInstructionText(pipeline.fetchPhaseInst));
        printf("  ID:  %s\n", getInstructionText(pipeline.decodePhaseInst));
        printf("  EX:  %s\n", getInstructionText(pipeline.executePhaseInst));
        printf("  MEM: %s\n", getInstructionText(pipeline.memoryPhaseInst));
        printf("  WB:  %s\n", getInstructionText(pipeline.writebackPhaseInst));
        
        // Print hazard status
        if (forwardingUnit.stall || structuralHazard.stall || branchPredictor.flushPipeline) {
            printf("  Hazard Status: ");
            if (forwardingUnit.stall) printf("Data Hazard ");
            if (structuralHazard.stall) printf("Structural Hazard ");
            if (branchPredictor.flushPipeline) printf("Control Hazard");
            printf("\n");
        }
        
        // Print register states
        printf("\nRegister State:\n");
        for (int i = 0; i < 8; i++) {
            printf("  R%d: %d", i, registers[i].word);
            if (i < 7) printf("\t");
            else printf("\n");
        }
        
        cycle++;
    }
    
    printf("\nPipeline execution completed after %d cycles.\n", cycle);
    
    // Print final register state
    printf("\nFinal Register State:\n");
    for (int i = 0; i < REGISTER_COUNT; i++) {
        if (i % 8 == 0) printf("\n");
        printf("R%d: %d\t", i, registers[i].word);
    }
    printf("\n");
}