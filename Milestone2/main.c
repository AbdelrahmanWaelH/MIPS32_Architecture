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

// Tracking arrays for reporting updates
uint32_t previous_registers[32]; // Previous values of registers for change reporting
uint32_t previous_memory[2048];  // Previous values of memory for change reporting
bool register_updated[32];       // Flags to indicate which registers were updated
bool memory_updated[2048];       // Flags to indicate which memory locations were updated

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
  int cycles_in_stage;    // cycles spent in this stage (used in decode and execute to detect the second cycle for execution)
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
const char *program_file = "test_combined_hazards.txt";

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
// Delay in cycles
// Read the last pages of the project !
// The prints should be updated to match the requirement

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
      pipeline[WB_STAGE].active = 0;
    }

    cycle++;
  }

  printf("\nSimulation completed in %d cycles.\n", cycle - 1);
  
  // Display all register contents after last cycle
  printf("Final register values (all registers):\n");
  for (int i = 0; i < 32; i++) {
    printf("R%d = %d (0x%08X)\n", i, registers[i], registers[i]);
  }
  
  // Display full memory contents after last cycle
  printf("\nFinal memory contents:\n");
  
  // Instruction memory (first 1024 words)
  printf("Instruction Memory:\n");
  for (int i = 0; i < 1024; i++) {
    if (memory[i] != 0) {
      printf("Memory[%d] = 0x%08X\n", i, memory[i]);
    }
  }
  
  // Data memory (next 1024 words)
  printf("\nData Memory:\n");
  for (int i = 1024; i < 2048; i++) {
    if (memory[i] != 0) {
      printf("Memory[%d] = %d (0x%08X)\n", i, memory[i], memory[i]);
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
  
  // Initialize tracking arrays
  memset(previous_registers, 0, sizeof(previous_registers));
  memset(previous_memory, 0, sizeof(previous_memory));
  memset(register_updated, 0, sizeof(register_updated));
  memset(memory_updated, 0, sizeof(memory_updated));
}

void fetch_instruction() {
  // Check if ID stage is stalled - don't fetch new instructions
  if (pipeline[ID_STAGE].stall_cycles > 0) {
    // Keep the current instruction in the IF stage
    return;
  }
  
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
  // If we're stalling, don't perform decoding
  if (pipeline[ID_STAGE].stall_cycles > 0) {
    // Don't move instruction from IF to ID when stalling
    return;
  }

  // Move instruction from IF to ID if the ID stage is now free
  if (!pipeline[ID_STAGE].active && pipeline[IF_STAGE].active) {
    memcpy(&pipeline[ID_STAGE], &pipeline[IF_STAGE], sizeof(PipelineReg));
    pipeline[ID_STAGE].cycles_in_stage = 0;
    pipeline[IF_STAGE].active = 0;
  }

  // Process instruction already in ID stage
  if (pipeline[ID_STAGE].active) {
    pipeline[ID_STAGE].cycles_in_stage++;

    // Only perform decoding on the second cycle in ID stage
    if (pipeline[ID_STAGE].cycles_in_stage == 2) {
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
          // SW format: SW Rs Rd imm => MEM[Rd + imm] = Rs
          // In our encoding: opcode(4) | rs(5) | rd(5) | immediate(18)
          // where rs is the source register (whose value is stored)
          // and rd is the base address register
          pipeline[ID_STAGE].mem_write = 1;
          pipeline[ID_STAGE].rs2 = pipeline[ID_STAGE].rd; // rs is in rd field
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
}

void execute_instruction() {
  // Process instruction already in EX stage
  if (pipeline[EX_STAGE].active) {
    pipeline[EX_STAGE].cycles_in_stage++;

    // Check for forwarding from MEM stage before executing
    if (pipeline[MEM_STAGE].active && pipeline[MEM_STAGE].reg_write && pipeline[MEM_STAGE].rd > 0) {
      // Forward from MEM to EX if needed
      if (pipeline[EX_STAGE].rs1 == pipeline[MEM_STAGE].rd) {
        pipeline[EX_STAGE].rs1_value = pipeline[MEM_STAGE].result;
      }
      if (pipeline[EX_STAGE].rs2 == pipeline[MEM_STAGE].rd) {
        pipeline[EX_STAGE].rs2_value = pipeline[MEM_STAGE].result;
      }
    }

    // Only perform execution on the second cycle in EX stage
    if (pipeline[EX_STAGE].cycles_in_stage == 2) {
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
            pipeline[EX_STAGE].rs1_value + pipeline[EX_STAGE].immediate + 1024;
        break;
      case SW:
        // SW: MEM[rs1 + imm] = rs2_value
        pipeline[EX_STAGE].mem_address =
            pipeline[EX_STAGE].rs1_value + pipeline[EX_STAGE].immediate + 1024;
        break;
      }
    }

    // Check if we're ready to move to the next stage
    if (pipeline[EX_STAGE].cycles_in_stage < 2) {
      // Not ready yet - still in first cycle
      return;
    }
  }

  // Move instruction from ID to EX if the EX stage is now free and ID has completed
  if (!pipeline[EX_STAGE].active && pipeline[ID_STAGE].active &&
      pipeline[ID_STAGE].cycles_in_stage >= 2 && pipeline[ID_STAGE].stall_cycles == 0) {
    memcpy(&pipeline[EX_STAGE], &pipeline[ID_STAGE], sizeof(PipelineReg));
    pipeline[EX_STAGE].cycles_in_stage = 1; // Starting first cycle in EX stage
    pipeline[ID_STAGE].active = 0;
  }
}


void memory_access() {
  // Move instruction from EX to MEM only if EX has completed its 2 cycles
  if (pipeline[EX_STAGE].active && pipeline[EX_STAGE].cycles_in_stage >= 2) {
    memcpy(&pipeline[MEM_STAGE], &pipeline[EX_STAGE], sizeof(PipelineReg));
    pipeline[MEM_STAGE].cycles_in_stage = 1;
    pipeline[EX_STAGE].active = 0;

    // Memory operations
    if (pipeline[MEM_STAGE].mem_read) {
      // Load word
      if (pipeline[MEM_STAGE].mem_address >= 0 &&
          pipeline[MEM_STAGE].mem_address < 2048) {
        // Read value from memory
        pipeline[MEM_STAGE].result = memory[pipeline[MEM_STAGE].mem_address];
        
        // Forwarding from MEM to EX (if needed)
        // If the EX stage needs the value we just loaded, forward it directly
        if (pipeline[EX_STAGE].active) {
          if (pipeline[EX_STAGE].rs1 > 0 && pipeline[EX_STAGE].rs1 == pipeline[MEM_STAGE].rd) {
            pipeline[EX_STAGE].rs1_value = pipeline[MEM_STAGE].result;
          }
          if (pipeline[EX_STAGE].rs2 > 0 && pipeline[EX_STAGE].rs2 == pipeline[MEM_STAGE].rd) {
            pipeline[EX_STAGE].rs2_value = pipeline[MEM_STAGE].result;
          }
        }
        
        // Forward to ID stage as well if needed
        if (pipeline[ID_STAGE].active && pipeline[ID_STAGE].cycles_in_stage > 0) {
          if (pipeline[ID_STAGE].rs1 > 0 && pipeline[ID_STAGE].rs1 == pipeline[MEM_STAGE].rd) {
            pipeline[ID_STAGE].rs1_value = pipeline[MEM_STAGE].result;
          }
          if (pipeline[ID_STAGE].rs2 > 0 && pipeline[ID_STAGE].rs2 == pipeline[MEM_STAGE].rd) {
            pipeline[ID_STAGE].rs2_value = pipeline[MEM_STAGE].result;
          }
        }
      } else {
        printf("Memory access error: address %d is out of bounds\n",
               pipeline[MEM_STAGE].mem_address);
      }
    } else if (pipeline[MEM_STAGE].mem_write) {
      // Store word
      if (pipeline[MEM_STAGE].mem_address >= 0 &&
          pipeline[MEM_STAGE].mem_address < 2048) {
        // Save the previous value for reporting
        previous_memory[pipeline[MEM_STAGE].mem_address] = memory[pipeline[MEM_STAGE].mem_address];
        // Mark this memory location as updated
        memory_updated[pipeline[MEM_STAGE].mem_address] = true;
        
        // For SW instruction, rs2_value contains the value to store
        memory[pipeline[MEM_STAGE].mem_address] = pipeline[MEM_STAGE].rs2_value;
      } else {
        printf("Memory access error: address %d is out of bounds\n",
               pipeline[MEM_STAGE].mem_address);
      }
    }
  }
}

void write_back() {
  // Move instruction from MEM to WB
  if (pipeline[MEM_STAGE].active) {
    memcpy(&pipeline[WB_STAGE], &pipeline[MEM_STAGE], sizeof(PipelineReg));
    pipeline[MEM_STAGE].active = 0;
  }else {
    pipeline[WB_STAGE].active = 0;
  }

  // Write back to register file
  if (pipeline[WB_STAGE].active && pipeline[WB_STAGE].reg_write &&
      pipeline[WB_STAGE].rd > 0) {
    // Save the previous value for reporting
    previous_registers[pipeline[WB_STAGE].rd] = registers[pipeline[WB_STAGE].rd];
    // Mark this register as updated
    register_updated[pipeline[WB_STAGE].rd] = true;
    // Update the register
    registers[pipeline[WB_STAGE].rd] = pipeline[WB_STAGE].result;
  }
}
void detect_hazards() {
  bool need_stall = false;

  // Check for hazards in the ID stage
  if (pipeline[ID_STAGE].active && pipeline[ID_STAGE].cycles_in_stage > 0) {
    
    // Check for data hazards with EX stage (forwarding from EX to ID)
    if (pipeline[EX_STAGE].active && pipeline[EX_STAGE].reg_write && pipeline[EX_STAGE].rd > 0) {
      // Forward from EX to ID if values are ready
      if (pipeline[EX_STAGE].cycles_in_stage == 2) {
        if (pipeline[ID_STAGE].rs1 == pipeline[EX_STAGE].rd) {
          pipeline[ID_STAGE].rs1_value = pipeline[EX_STAGE].result;
        }
        if (pipeline[ID_STAGE].rs2 == pipeline[EX_STAGE].rd) {
          pipeline[ID_STAGE].rs2_value = pipeline[EX_STAGE].result;
        }
      }
    }
    
    // Check for data hazards with MEM stage
    if (pipeline[MEM_STAGE].active && pipeline[MEM_STAGE].reg_write && pipeline[MEM_STAGE].rd > 0) {
      // Handle forwarding from MEM to ID
      if (pipeline[ID_STAGE].rs1 == pipeline[MEM_STAGE].rd) {
        pipeline[ID_STAGE].rs1_value = pipeline[MEM_STAGE].result;
      }
      if (pipeline[ID_STAGE].rs2 == pipeline[MEM_STAGE].rd) {
        pipeline[ID_STAGE].rs2_value = pipeline[MEM_STAGE].result;
      }
    }
    
    // Special case: If SW in ID depends on LW in EX (SW with source register being loaded)
    if (pipeline[ID_STAGE].opcode == SW && pipeline[EX_STAGE].active && 
        pipeline[EX_STAGE].opcode == LW && pipeline[EX_STAGE].rd > 0) {
      // If SW source register (rs2) is equal to LW destination register (rd)
      if (pipeline[ID_STAGE].rs2 == pipeline[EX_STAGE].rd) {
        // Need to stall until load completes
        pipeline[ID_STAGE].stall_cycles = 2;
        need_stall = true;
      }
    }
    
    // Check for load-use hazards (LW followed immediately by an instruction that uses the loaded value)
    if (pipeline[EX_STAGE].active && pipeline[EX_STAGE].opcode == LW && pipeline[EX_STAGE].rd > 0) {
      // Check if ID stage depends on a value being loaded in EX
      if ((pipeline[ID_STAGE].rs1 == pipeline[EX_STAGE].rd && pipeline[ID_STAGE].rs1 > 0) ||
          (pipeline[ID_STAGE].rs2 == pipeline[EX_STAGE].rd && pipeline[ID_STAGE].rs2 > 0)) {
        // Stall for 2 cycles (or until LW completes memory stage)
        pipeline[ID_STAGE].stall_cycles = 2;
        need_stall = true;
      }
    }
  }

  // Handle stalling
  if (need_stall) {
    // Keep ID stage active but don't allow it to progress
    return;
  } else if (pipeline[ID_STAGE].stall_cycles > 0) {
    // We're still stalling from a previous hazard
    pipeline[ID_STAGE].stall_cycles--;
  }
}

// Arrays to track register and memory updates for reporting
uint32_t previous_registers[32];
uint32_t previous_memory[2048];
bool register_updated[32];
bool memory_updated[2048];

void display_processor_state(int cycle) {
  // First print the clock cycle number
  printf("======= Clock Cycle %d =======\n", cycle);
  printf("  PC: %d\n", PC);

  // Reset update trackers for this cycle
  memset(register_updated, 0, sizeof(register_updated));
  memset(memory_updated, 0, sizeof(memory_updated));

  printf("  Pipeline Stages:\n");
  
  // IF Stage
  if (pipeline[IF_STAGE].active) {
    printf("    IF: Instruction at address %d (PC=%d)\n", 
           pipeline[IF_STAGE].PC, pipeline[IF_STAGE].PC);
    printf("        Input Parameters: PC=%d\n", pipeline[IF_STAGE].PC);
  } else {
    printf("    IF: Idle\n");
  }

  // ID Stage
  if (pipeline[ID_STAGE].active) {
    printf("    ID: ");
    print_instruction(pipeline[ID_STAGE].opcode, pipeline[ID_STAGE].rd,
                      pipeline[ID_STAGE].rs1, pipeline[ID_STAGE].rs2,
                      pipeline[ID_STAGE].immediate, pipeline[ID_STAGE].shamt,
                      pipeline[ID_STAGE].address);
    printf(" (cycle %d of 2)", pipeline[ID_STAGE].cycles_in_stage);
    if (pipeline[ID_STAGE].stall_cycles > 0) {
      printf(" (stalled)");
    }
    printf("\n");
    printf("        Input Parameters: Instruction=0x%08X, PC=%d\n", 
           pipeline[ID_STAGE].instruction, pipeline[ID_STAGE].PC);
    if (pipeline[ID_STAGE].cycles_in_stage == 2) {
      if (pipeline[ID_STAGE].rs1 > 0) {
        printf("        RS1(R%d)=%d, ", pipeline[ID_STAGE].rs1, pipeline[ID_STAGE].rs1_value);
      }
      if (pipeline[ID_STAGE].rs2 > 0 && (pipeline[ID_STAGE].instr_type == R_TYPE || 
          pipeline[ID_STAGE].opcode == BNE || pipeline[ID_STAGE].opcode == SW)) {
        printf("RS2(R%d)=%d, ", pipeline[ID_STAGE].rs2, pipeline[ID_STAGE].rs2_value);
      }
      if (pipeline[ID_STAGE].instr_type == I_TYPE && pipeline[ID_STAGE].opcode != BNE && 
          pipeline[ID_STAGE].opcode != SW) {
        printf("IMM=%d, ", pipeline[ID_STAGE].immediate);
      }
      if (pipeline[ID_STAGE].instr_type == R_TYPE && 
          (pipeline[ID_STAGE].opcode == SLL || pipeline[ID_STAGE].opcode == SRL)) {
        printf("SHAMT=%d, ", pipeline[ID_STAGE].shamt);
      }
      if (pipeline[ID_STAGE].instr_type == J_TYPE) {
        printf("ADDR=%d, ", pipeline[ID_STAGE].address);
      }
      printf("\n");
    }
  } else {
    printf("    ID: Idle\n");
  }

  // EX Stage
  if (pipeline[EX_STAGE].active) {
    printf("    EX: ");
    print_instruction(pipeline[EX_STAGE].opcode, pipeline[EX_STAGE].rd,
                      pipeline[EX_STAGE].rs1, pipeline[EX_STAGE].rs2,
                      pipeline[EX_STAGE].immediate, pipeline[EX_STAGE].shamt,
                      pipeline[EX_STAGE].address);
    printf(" (cycle %d of 2)", pipeline[EX_STAGE].cycles_in_stage);
    printf("\n");
    printf("        Input Parameters: ");
    if (pipeline[EX_STAGE].rs1 > 0) {
      printf("RS1(R%d)=%d, ", pipeline[EX_STAGE].rs1, pipeline[EX_STAGE].rs1_value);
    }
    if (pipeline[EX_STAGE].rs2 > 0 && (pipeline[EX_STAGE].instr_type == R_TYPE || 
        pipeline[EX_STAGE].opcode == BNE || pipeline[EX_STAGE].opcode == SW)) {
      printf("RS2(R%d)=%d, ", pipeline[EX_STAGE].rs2, pipeline[EX_STAGE].rs2_value);
    }
    if (pipeline[EX_STAGE].instr_type == I_TYPE && pipeline[EX_STAGE].opcode != BNE && 
        pipeline[EX_STAGE].opcode != SW) {
      printf("IMM=%d, ", pipeline[EX_STAGE].immediate);
    }
    if (pipeline[EX_STAGE].instr_type == R_TYPE && 
        (pipeline[EX_STAGE].opcode == SLL || pipeline[EX_STAGE].opcode == SRL)) {
      printf("SHAMT=%d, ", pipeline[EX_STAGE].shamt);
    }
    if (pipeline[EX_STAGE].cycles_in_stage == 2) {
      printf("\n        Result=%d", pipeline[EX_STAGE].result);
      if (pipeline[EX_STAGE].branch_taken) {
        printf(", Branch Taken (new PC=%d)", pipeline[EX_STAGE].PC + 1 + pipeline[EX_STAGE].immediate);
      }
    }
    printf("\n");
  } else {
    printf("    EX: Idle\n");
  }

  // MEM Stage
  if (cycle % 2 == 0) { // Even cycles have MEM stage
    if (pipeline[MEM_STAGE].active) {
      printf("    MEM: ");
      print_instruction(pipeline[MEM_STAGE].opcode, pipeline[MEM_STAGE].rd,
                        pipeline[MEM_STAGE].rs1, pipeline[MEM_STAGE].rs2,
                        pipeline[MEM_STAGE].immediate,
                        pipeline[MEM_STAGE].shamt, pipeline[MEM_STAGE].address);
      printf("\n");
      printf("        Input Parameters: ");
      if (pipeline[MEM_STAGE].mem_read || pipeline[MEM_STAGE].mem_write) {
        printf("Memory Address=%d, ", pipeline[MEM_STAGE].mem_address);
      }
      
      if (pipeline[MEM_STAGE].mem_write) {
        // For SW instruction, rs2_value contains the value to store
        printf("Value to Store=%d", pipeline[MEM_STAGE].rs2_value);
        // Track memory update
        memory_updated[pipeline[MEM_STAGE].mem_address] = true;
        previous_memory[pipeline[MEM_STAGE].mem_address] = memory[pipeline[MEM_STAGE].mem_address];
      } else if (pipeline[MEM_STAGE].mem_read) {
        printf("Loaded Value=%d", pipeline[MEM_STAGE].result);
      }
      printf("\n");
    } else {
      printf("    MEM: Idle\n");
    }
  } else {
    printf("    MEM: Disabled in odd cycles\n");
  }

  // WB Stage
  if (pipeline[WB_STAGE].active) {
    printf("    WB: ");
    print_instruction(pipeline[WB_STAGE].opcode, pipeline[WB_STAGE].rd,
                      pipeline[WB_STAGE].rs1, pipeline[WB_STAGE].rs2,
                      pipeline[WB_STAGE].immediate, pipeline[WB_STAGE].shamt,
                      pipeline[WB_STAGE].address);
    printf("\n");
    printf("        Input Parameters: ");
    if (pipeline[WB_STAGE].reg_write && pipeline[WB_STAGE].rd > 0) {
      printf("Result=%d to write to R%d", pipeline[WB_STAGE].result, pipeline[WB_STAGE].rd);
      // Track register update
      register_updated[pipeline[WB_STAGE].rd] = true;
      previous_registers[pipeline[WB_STAGE].rd] = registers[pipeline[WB_STAGE].rd];
    }
    printf("\n");
  } else {
    printf("    WB: Idle\n");
  }
  
  // Display register updates
  bool any_reg_updated = false;
  for (int i = 0; i < 32; i++) {
    if (register_updated[i]) {
      if (!any_reg_updated) {
        printf("  Register Updates:\n");
        any_reg_updated = true;
      }
      printf("    R%d: %d → %d\n", i, previous_registers[i], registers[i]);
    }
  }
  
  // Display memory updates
  bool any_mem_updated = false;
  for (int i = 0; i < 2048; i++) {
    if (memory_updated[i]) {
      if (!any_mem_updated) {
        printf("  Memory Updates:\n");
        any_mem_updated = true;
      }
      printf("    Memory[%d]: %d → %d\n", i, previous_memory[i], memory[i]);
    }
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
    printf("SW R%d R%d %d", rs2, rs1, imm);  // SW rs rd imm = MEM[rd + imm] = rs
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