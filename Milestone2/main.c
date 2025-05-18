#include "FileReader.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Define opcodes
#define ADD 0
#define SUB 1
#define MULI 2
#define ADDI 3
#define BNE 4
#define ANDI 5
#define ORI 6
#define J 7
#define SLL 8
#define SRL 9
#define LW 10
#define SW 11

// Define instruction types
#define R_TYPE 0
#define I_TYPE 1
#define J_TYPE 2

// Define pipeline stages
#define IF_STAGE 0
#define ID_STAGE 1
#define EX_STAGE 2
#define MEM_STAGE 3
#define WB_STAGE 4

// Processor components
uint32_t memory[2048];  // 2048 words (32 bits each)
uint32_t registers[32]; // 31 GPRs + 1 Zero register
uint32_t PC;            // Program Counter

// Pipeline registers
typedef struct {
  int active;           // Is this pipeline stage active?
  uint32_t instruction; // The instruction being processed
  uint32_t PC;          // PC value for this instruction
  int opcode;           // Instruction opcode
  int rs1;              // Source register 1
  int rs2;              // Source register 2
  int rd;               // Destination register
  int immediate;        // Immediate value
  int shamt;            // Shift amount
  int address;          // Jump address
  uint32_t rs1_value;   // Value of source register 1
  uint32_t rs2_value;   // Value of source register 2
  uint32_t result;      // Result of operation
  int instr_type;       // Instruction type (R, I, J)
  int mem_read;         // Does this instruction read memory?
  int mem_write;        // Does this instruction write memory?
  int reg_write;        // Does this instruction write to a register?
  int branch_taken;     // Was a branch taken?
  int mem_address;      // Memory address to access
  int stall_cycles;     // Number of cycles to stall
} PipelineReg;

PipelineReg pipeline[5]; // One for each stage

// Forward declarations
void initialize();
void fetch_instruction();
void decode_instruction();
void execute_instruction();
void memory_access();
void write_back();
void detect_hazards();
void display_processor_state(int cycle);
void print_instruction(int opcode, int rd, int rs1, int rs2, int imm, int shamt,
                       int addr);
int load_program_from_file(const char *filename);

// Helper function to get opcode from mnemonic
int get_opcode(const char *mnemonic) {
  if (strcmp(mnemonic, "ADD") == 0)
    return ADD;
  if (strcmp(mnemonic, "SUB") == 0)
    return SUB;
  if (strcmp(mnemonic, "MULI") == 0)
    return MULI;
  if (strcmp(mnemonic, "ADDI") == 0)
    return ADDI;
  if (strcmp(mnemonic, "BNE") == 0)
    return BNE;
  if (strcmp(mnemonic, "ANDI") == 0)
    return ANDI;
  if (strcmp(mnemonic, "ORI") == 0)
    return ORI;
  if (strcmp(mnemonic, "J") == 0)
    return J;
  if (strcmp(mnemonic, "SLL") == 0)
    return SLL;
  if (strcmp(mnemonic, "SRL") == 0)
    return SRL;
  if (strcmp(mnemonic, "LW") == 0)
    return LW;
  if (strcmp(mnemonic, "SW") == 0)
    return SW;
  return -1; // Invalid mnemonic
}
const char *program_file = "../FahmyCodeTest";

// Helper function to parse register number
int parse_register(const char *reg_str) {
  // Skip 'R' prefix
  if (reg_str[0] == 'R' || reg_str[0] == 'r') {
    return atoi(reg_str + 1);
  }
  return -1; // Invalid register
}

bool check_running(int total_instructions) {
  return PC < total_instructions || pipeline[0].active || pipeline[1].active ||
         pipeline[2].active || pipeline[3].active || pipeline[4].active;
}

int main() {
  initialize();
  int total_instructions = load_program_from_file(program_file);

  if (total_instructions <= 0) {
    printf("Error loading program or no instructions found.\n");
    return 1;
  }

  int cycle = 1;
  int instructions_executed = 0;

  printf("Program loaded with %d instructions\n", total_instructions);

  // Calculate total cycles: 7 + ((n - 1) * 2)
  int total_cycles = 7 + ((total_instructions - 1) * 2);
  printf("Estimated total cycles: %d\n\n", total_cycles);

  // Run the simulation
  while (check_running(total_instructions)) {
    printf("Cycle %d:\n", cycle);

    // Detect and resolve hazards first
    detect_hazards();

    // Execute pipeline stages (in reverse to prevent overwriting)
    if (cycle % 2 == 1) { // Odd cycles: IF, ID, EX, WB
      write_back();
      execute_instruction();
      decode_instruction();
      fetch_instruction();
    } else { // Even cycles: ID, EX, MEM, WB
      write_back();
      memory_access();
      execute_instruction();
      decode_instruction();
    }

    // Display state after this cycle
    display_processor_state(cycle);

    // Count completed instructions
    if (pipeline[WB_STAGE].active) {
      instructions_executed++;
    }

    cycle++;
  }

  printf("\nSimulation completed in %d cycles.\n", cycle - 1);
  printf("Final register values:\n");
  for (int i = 0; i < 32; i++) {
    if (registers[i] != 0) {
      printf("R%d = %d (0x%08X)\n", i, registers[i], registers[i]);
    }
  }

  return 0;
}

void initialize() {
  // Clear memory
  memset(memory, 0, sizeof(memory));

  // Clear registers
  memset(registers, 0, sizeof(registers));

  // Initialize PC to 0
  PC = 0;

  // Initialize pipeline registers
  for (int i = 0; i < 5; i++) {
    memset(&pipeline[i], 0, sizeof(PipelineReg));
  }
}

void fetch_instruction() {
  // Only fetch if we haven't reached the end of program
  if (PC < 1024 && memory[PC] != 0) {
    pipeline[IF_STAGE].active = 1;
    pipeline[IF_STAGE].instruction = memory[PC];
    pipeline[IF_STAGE].PC = PC;

    // Increment PC for next instruction
    PC++;
  } else {
    pipeline[IF_STAGE].active = 0;
  }
}

void decode_instruction() {
  // Move instruction from IF to ID
  if (pipeline[IF_STAGE].active) {
    memcpy(&pipeline[ID_STAGE], &pipeline[IF_STAGE], sizeof(PipelineReg));
    pipeline[IF_STAGE].active = 0;

    uint32_t instr = pipeline[ID_STAGE].instruction;
    pipeline[ID_STAGE].opcode = (instr >> 28) & 0xF; // First 4 bits

    // Determine instruction type and decode fields
    switch (pipeline[ID_STAGE].opcode) {
    case ADD:
    case SUB:
    case SLL:
    case SRL:
      // R-type: opcode(4) | rd(5) | rs1(5) | rs2(5) | shamt(5) | unused(8)
      pipeline[ID_STAGE].instr_type = R_TYPE;
      pipeline[ID_STAGE].rd = (instr >> 23) & 0x1F;
      pipeline[ID_STAGE].rs1 = (instr >> 18) & 0x1F;
      pipeline[ID_STAGE].rs2 = (instr >> 13) & 0x1F;
      pipeline[ID_STAGE].shamt = (instr >> 8) & 0x1F;
      pipeline[ID_STAGE].reg_write = 1;
      break;

    case MULI:
    case ADDI:
    case BNE:
    case ANDI:
    case ORI:
    case LW:
    case SW:
      // I-type: opcode(4) | rd(5) | rs1(5) | immediate(18)
      pipeline[ID_STAGE].instr_type = I_TYPE;
      pipeline[ID_STAGE].rd = (instr >> 23) & 0x1F;
      pipeline[ID_STAGE].rs1 = (instr >> 18) & 0x1F;
      pipeline[ID_STAGE].immediate = instr & 0x3FFFF; // 18 bits

      // Sign extend the immediate value if needed
      if ((pipeline[ID_STAGE].immediate & 0x20000) >> 17 == 1) {
        pipeline[ID_STAGE].immediate |= 0xFFFC0000; // Sign extend to 32 bits
      }

      // Set flags for memory operations
      if (pipeline[ID_STAGE].opcode == LW) {
        pipeline[ID_STAGE].mem_read = 1;
        pipeline[ID_STAGE].reg_write = 1;
      } else if (pipeline[ID_STAGE].opcode == SW) {
        pipeline[ID_STAGE].mem_write = 1;
        pipeline[ID_STAGE].rs2 =
            pipeline[ID_STAGE].rd; // For SW, "rd" is actually rs2
        pipeline[ID_STAGE].rd = 0; // SW doesn't write to a register
      } else if (pipeline[ID_STAGE].opcode == BNE) {
        pipeline[ID_STAGE].rs2 =
            pipeline[ID_STAGE].rd; // For BNE, "rd" is actually rs2
        pipeline[ID_STAGE].rd = 0; // BNE doesn't write to a register
      } else {
        pipeline[ID_STAGE].reg_write = 1;
      }
      break;

    case J:
      // J-type: opcode(4) | address(28)
      pipeline[ID_STAGE].instr_type = J_TYPE;
      pipeline[ID_STAGE].address = instr & 0x0FFFFFFF; // 28 bits
      break;
    }

    // Read register values
    if (pipeline[ID_STAGE].rs1 > 0) {
      pipeline[ID_STAGE].rs1_value = registers[pipeline[ID_STAGE].rs1];
    }

    if (pipeline[ID_STAGE].rs2 > 0) {
      pipeline[ID_STAGE].rs2_value = registers[pipeline[ID_STAGE].rs2];
    }
  }
}

void execute_instruction() {
  // Move instruction from ID to EX
  if (pipeline[ID_STAGE].active && pipeline[ID_STAGE].stall_cycles == 0) {
    memcpy(&pipeline[EX_STAGE], &pipeline[ID_STAGE], sizeof(PipelineReg));
    pipeline[ID_STAGE].active = 0;

    // Execute based on opcode
    switch (pipeline[EX_STAGE].opcode) {
    case ADD:
      pipeline[EX_STAGE].result =
          pipeline[EX_STAGE].rs1_value + pipeline[EX_STAGE].rs2_value;
      break;

    case SUB:
      pipeline[EX_STAGE].result =
          pipeline[EX_STAGE].rs1_value - pipeline[EX_STAGE].rs2_value;
      break;

    case MULI:
      pipeline[EX_STAGE].result =
          pipeline[EX_STAGE].rs1_value * pipeline[EX_STAGE].immediate;
      break;

    case ADDI:
      pipeline[EX_STAGE].result =
          pipeline[EX_STAGE].rs1_value + pipeline[EX_STAGE].immediate;
      break;

    case BNE:
      if (pipeline[EX_STAGE].rs1_value != pipeline[EX_STAGE].rs2_value) {
        // Branch is taken
        PC = pipeline[EX_STAGE].PC + 1 + pipeline[EX_STAGE].immediate;
        pipeline[EX_STAGE].branch_taken = 1;

        // Flush the pipeline
        pipeline[IF_STAGE].active = 0;
        pipeline[ID_STAGE].active = 0;
      }
      break;

    case ANDI:
      pipeline[EX_STAGE].result =
          pipeline[EX_STAGE].rs1_value & pipeline[EX_STAGE].immediate;
      break;

    case ORI:
      pipeline[EX_STAGE].result =
          pipeline[EX_STAGE].rs1_value | pipeline[EX_STAGE].immediate;
      break;

    case J:
      // Jump: PC = PC[31:28] || ADDRESS
      PC = (PC & 0xF0000000) | pipeline[EX_STAGE].address;

      // Flush the pipeline
      pipeline[IF_STAGE].active = 0;
      pipeline[ID_STAGE].active = 0;
      break;

    case SLL:
      pipeline[EX_STAGE].result = pipeline[EX_STAGE].rs1_value
                                  << pipeline[EX_STAGE].shamt;
      break;

    case SRL:
      pipeline[EX_STAGE].result =
          pipeline[EX_STAGE].rs1_value >> pipeline[EX_STAGE].shamt;
      break;

    case LW:
      pipeline[EX_STAGE].mem_address =
          pipeline[EX_STAGE].rs2_value + pipeline[EX_STAGE].immediate + 1024;
      break;

    case SW:
      pipeline[EX_STAGE].mem_address =
          pipeline[EX_STAGE].rs2_value + pipeline[EX_STAGE].immediate + 1024;
      break;
    }
  }
}

void memory_access() {
  // Move instruction from EX to MEM
  if (pipeline[EX_STAGE].active) {
    memcpy(&pipeline[MEM_STAGE], &pipeline[EX_STAGE], sizeof(PipelineReg));
    pipeline[EX_STAGE].active = 0;

    // Memory operations
    if (pipeline[MEM_STAGE].mem_read) {
      // Load word
      if (pipeline[MEM_STAGE].mem_address >= 0 &&
          pipeline[MEM_STAGE].mem_address < 2048) {
        pipeline[MEM_STAGE].result = memory[pipeline[MEM_STAGE].mem_address];
      } else {
        printf("Memory access error: address %d is out of bounds\n",
               pipeline[MEM_STAGE].mem_address);
      }
    } else if (pipeline[MEM_STAGE].mem_write) {
      // Store word
      if (pipeline[MEM_STAGE].mem_address >= 0 &&
          pipeline[MEM_STAGE].mem_address < 2048) {
        memory[pipeline[MEM_STAGE].mem_address] = pipeline[MEM_STAGE].rs1_value;
      } else {
        printf("Memory access error: address %d is out of bounds\n",
               pipeline[MEM_STAGE].mem_address);
      }
    }
  }
}

void write_back() {
  // Move instruction from MEM to WB (or from EX to WB if no MEM stage is
  // active)
  if (pipeline[MEM_STAGE].active) {
    memcpy(&pipeline[WB_STAGE], &pipeline[MEM_STAGE], sizeof(PipelineReg));
    pipeline[MEM_STAGE].active = 0;
  } else if (pipeline[EX_STAGE].active && !pipeline[EX_STAGE].mem_read &&
             !pipeline[EX_STAGE].mem_write) {
    // Skip MEM stage if not needed
    memcpy(&pipeline[WB_STAGE], &pipeline[EX_STAGE], sizeof(PipelineReg));
    pipeline[EX_STAGE].active = 0;
  } else {
    pipeline[WB_STAGE].active = 0;
  }

  // Write back to register file
  if (pipeline[WB_STAGE].active && pipeline[WB_STAGE].reg_write &&
      pipeline[WB_STAGE].rd > 0) {
    registers[pipeline[WB_STAGE].rd] = pipeline[WB_STAGE].result;
  }
}

void detect_hazards() {
  // Data hazards detection (RAW)

  // Check if ID stage needs a value that's being produced in EX stage
  if (pipeline[ID_STAGE].active && pipeline[EX_STAGE].active &&
      pipeline[EX_STAGE].reg_write && pipeline[EX_STAGE].rd > 0) {

    // Check if rs1 in ID depends on rd in EX
    if (pipeline[ID_STAGE].rs1 == pipeline[EX_STAGE].rd) {
      // Forward from EX to ID
      pipeline[ID_STAGE].rs1_value = pipeline[EX_STAGE].result;
    }

    // Check if rs2 in ID depends on rd in EX
    if (pipeline[ID_STAGE].rs2 == pipeline[EX_STAGE].rd) {
      // Forward from EX to ID
      pipeline[ID_STAGE].rs2_value = pipeline[EX_STAGE].result;
    }
  }

  // Check if ID stage needs a value that's being loaded from memory (LW in MEM
  // stage)
  if (pipeline[ID_STAGE].active && pipeline[MEM_STAGE].active &&
      pipeline[MEM_STAGE].mem_read && pipeline[MEM_STAGE].rd > 0) {

    // Check if rs1 or rs2 in ID depends on rd in MEM
    if (pipeline[ID_STAGE].rs1 == pipeline[MEM_STAGE].rd ||
        pipeline[ID_STAGE].rs2 == pipeline[MEM_STAGE].rd) {

      // Stall the ID stage until the value is available
      pipeline[ID_STAGE].stall_cycles = 1;
      return;
    }
  }

  // Decrement stall cycles if still stalling
  if (pipeline[ID_STAGE].stall_cycles > 0) {
    pipeline[ID_STAGE].stall_cycles--;
  }
}

void display_processor_state(int cycle) {
  printf("  PC: %d\n", PC);

  printf("  Pipeline:\n");
  if (pipeline[IF_STAGE].active) {
    printf("    IF: Instruction at address %d\n", pipeline[IF_STAGE].PC);
  } else {
    printf("    IF: Idle\n");
  }

  if (pipeline[ID_STAGE].active) {
    printf("    ID: ");
    print_instruction(pipeline[ID_STAGE].opcode, pipeline[ID_STAGE].rd,
                      pipeline[ID_STAGE].rs1, pipeline[ID_STAGE].rs2,
                      pipeline[ID_STAGE].immediate, pipeline[ID_STAGE].shamt,
                      pipeline[ID_STAGE].address);
    if (pipeline[ID_STAGE].stall_cycles > 0) {
      printf(" (stalled)");
    }
    printf("\n");
  } else {
    printf("    ID: Idle\n");
  }

  if (pipeline[EX_STAGE].active) {
    printf("    EX: ");
    print_instruction(pipeline[EX_STAGE].opcode, pipeline[EX_STAGE].rd,
                      pipeline[EX_STAGE].rs1, pipeline[EX_STAGE].rs2,
                      pipeline[EX_STAGE].immediate, pipeline[EX_STAGE].shamt,
                      pipeline[EX_STAGE].address);
    printf("\n");
  } else {
    printf("    EX: Idle\n");
  }

  if (cycle % 2 == 0) { // Even cycles have MEM stage
    if (pipeline[MEM_STAGE].active) {
      printf("    MEM: ");
      print_instruction(pipeline[MEM_STAGE].opcode, pipeline[MEM_STAGE].rd,
                        pipeline[MEM_STAGE].rs1, pipeline[MEM_STAGE].rs2,
                        pipeline[MEM_STAGE].immediate,
                        pipeline[MEM_STAGE].shamt, pipeline[MEM_STAGE].address);
      printf("\n");
    } else {
      printf("    MEM: Idle\n");
    }
  } else {
    printf("    MEM: Disabled in odd cycles\n");
  }

  if (pipeline[WB_STAGE].active) {
    printf("    WB: ");
    print_instruction(pipeline[WB_STAGE].opcode, pipeline[WB_STAGE].rd,
                      pipeline[WB_STAGE].rs1, pipeline[WB_STAGE].rs2,
                      pipeline[WB_STAGE].immediate, pipeline[WB_STAGE].shamt,
                      pipeline[WB_STAGE].address);
    if (pipeline[WB_STAGE].reg_write && pipeline[WB_STAGE].rd > 0) {
      printf(" (R%d = %d)", pipeline[WB_STAGE].rd, pipeline[WB_STAGE].result);
    }
    printf("\n");
  } else {
    printf("    WB: Idle\n");
  }
  printf("\n");
}

void print_instruction(int opcode, int rd, int rs1, int rs2, int imm, int shamt,
                       int addr) {
  switch (opcode) {
  case ADD:
    printf("ADD R%d R%d R%d", rd, rs1, rs2);
    break;
  case SUB:
    printf("SUB R%d R%d R%d", rd, rs1, rs2);
    break;
  case MULI:
    printf("MULI R%d R%d %d", rd, rs1, imm);
    break;
  case ADDI:
    printf("ADDI R%d R%d %d", rd, rs1, imm);
    break;
  case BNE:
    printf("BNE R%d R%d %d", rs1, rs2, imm);
    break;
  case ANDI:
    printf("ANDI R%d R%d %d", rd, rs1, imm);
    break;
  case ORI:
    printf("ORI R%d R%d %d", rd, rs1, imm);
    break;
  case J:
    printf("J %d", addr);
    break;
  case SLL:
    printf("SLL R%d R%d %d", rd, rs1, shamt);
    break;
  case SRL:
    printf("SRL R%d R%d %d", rd, rs1, shamt);
    break;
  case LW:
    printf("LW R%d R%d %d", rd, rs1, imm);
    break;
  case SW:
    printf("SW R%d R%d %d", rs2, rs1, imm);
    break;
  default:
    printf("Unknown instruction");
  }
}

// Function to load program from a text file
int load_program_from_file(const char *filename) {
  char *program_text = readFile((char *)filename);
  if (program_text == NULL) {
    return 0;
  }

  char *line = strtok(program_text, "\r\n");
  int instruction_count = 0;

  while (line != NULL && instruction_count < 1024) {
    char mnemonic[10] = {0};
    char operands[50] = {0};
    int result = sscanf(line, "%s %[^\n]", mnemonic, operands);

    if (result >= 1) {
      // Convert mnemonic to uppercase
      for (int i = 0; mnemonic[i]; i++) {
        mnemonic[i] = toupper(mnemonic[i]);
      }

      int opcode = get_opcode(mnemonic);
      if (opcode == -1) {
        printf("Invalid instruction mnemonic: %s\n", mnemonic);
        line = strtok(NULL, "\r\n");
        continue;
      }

      uint32_t instruction = 0;
      // Set opcode (4 bits at positions 31-28)
      instruction |= (opcode << 28);

      // Parse operands based on instruction type
      switch (opcode) {
      case ADD:
      case SUB: {
        // R-type with 3 registers
        char reg1[5], reg2[5], reg3[5];
        if (sscanf(operands, "%s %s %s", reg1, reg2, reg3) == 3) {
          int rd = parse_register(reg1);
          int rs1 = parse_register(reg2);
          int rs2 = parse_register(reg3);

          if (rd >= 0 && rs1 >= 0 && rs2 >= 0) {
            instruction |= (rd << 23);  // rd at positions 27-23
            instruction |= (rs1 << 18); // rs1 at positions 22-18
            instruction |= (rs2 << 13); // rs2 at positions 17-13
          } else {
            printf("Invalid register in instruction: %s %s\n", mnemonic,
                   operands);
            line = strtok(NULL, "\r\n");
            continue;
          }
        } else {
          printf("Invalid operands for %s: %s\n", mnemonic, operands);
          line = strtok(NULL, "\r\n");
          continue;
        }
        break;
      }

      case SLL:
      case SRL: {
        // R-type with 2 registers and shift amount
        char reg1[5], reg2[5];
        int shift;
        if (sscanf(operands, "%s %s %d", reg1, reg2, &shift) == 3) {
          int rd = parse_register(reg1);
          int rs1 = parse_register(reg2);

          if (rd >= 0 && rs1 >= 0) {
            instruction |= (rd << 23);   // rd at positions 27-23
            instruction |= (rs1 << 18);  // rs1 at positions 22-18
            instruction |= (0 << 13);    // rs2 = 0 for shift instructions
            instruction |= (shift << 8); // shamt at positions 12-8
          } else {
            printf("Invalid register in instruction: %s %s\n", mnemonic,
                   operands);
            line = strtok(NULL, "\r\n");
            continue;
          }
        } else {
          printf("Invalid operands for %s: %s\n", mnemonic, operands);
          line = strtok(NULL, "\r\n");
          continue;
        }
        break;
      }

      case MULI:
      case ADDI:
      case ANDI:
      case ORI:
      case LW:
      case SW: {
        // I-type with 2 registers and immediate
        char reg1[5], reg2[5];
        int imm;
        if (sscanf(operands, "%s %s %d", reg1, reg2, &imm) == 3) {
          int rd = parse_register(reg1);
          int rs1 = parse_register(reg2);

          if (rd >= 0 && rs1 >= 0) {
            instruction |= (rd << 23);      // rd at positions 27-23
            instruction |= (rs1 << 18);     // rs1 at positions 22-18
            instruction |= (imm & 0x3FFFF); // immediate (18 bits)
          } else {
            printf("Invalid register in instruction: %s %s\n", mnemonic,
                   operands);
            line = strtok(NULL, "\r\n");
            continue;
          }
        } else {
          printf("Invalid operands for %s: %s\n", mnemonic, operands);
          line = strtok(NULL, "\r\n");
          continue;
        }
        break;
      }

      case BNE: {
        // BNE with 2 registers and immediate
        char reg1[5], reg2[5];
        int imm;
        if (sscanf(operands, "%s %s %d", reg1, reg2, &imm) == 3) {
          int rs1 = parse_register(reg1);
          int rs2 = parse_register(reg2);

          if (rs1 >= 0 && rs2 >= 0) {
            instruction |=
                (rs2 << 23); // rs2 at positions 27-23 (using rd field)
            instruction |= (rs1 << 18);     // rs1 at positions 22-18
            instruction |= (imm & 0x3FFFF); // immediate (18 bits)
          } else {
            printf("Invalid register in instruction: %s %s\n", mnemonic,
                   operands);
            line = strtok(NULL, "\r\n");
            continue;
          }
        } else {
          printf("Invalid operands for %s: %s\n", mnemonic, operands);
          line = strtok(NULL, "\r\n");
          continue;
        }
        break;
      }

      case J: {
        // J-type with address
        int address;
        if (sscanf(operands, "%d", &address) == 1) {
          instruction |= (address & 0x0FFFFFFF); // address (28 bits)
        } else {
          printf("Invalid operands for %s: %s\n", mnemonic, operands);
          line = strtok(NULL, "\r\n");
          continue;
        }
        break;
      }
      }

      // Store the instruction in memory
      memory[instruction_count++] = instruction;
      printf("Loaded instruction %d: %s %s -> 0x%08X\n", instruction_count - 1,
             mnemonic, operands, instruction);
    }

    line = strtok(NULL, "\r\n");
  }

  free(program_text);
  return instruction_count;
}
