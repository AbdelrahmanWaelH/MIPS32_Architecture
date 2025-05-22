# ComputerSystemArchitecture
# 🧠 Pipelined MIPS32 CPU in C

A 5-stage pipelined MIPS32 CPU implemented from scratch in C, as part of a collaborative computer architecture project. Designed to simulate instruction-level parallelism, hazard handling, and control flow in a simplified MIPS processor.

## 📌 Features

- ✅ **5-stage pipeline**: IF, ID, EX, MEM, WB
- ✅ **12 MIPS instructions** supported
- ✅ **Hazard detection** and **data forwarding**
- ✅ **Branch handling** (with basic control hazard logic)
- ✅ **Cycle-by-cycle trace** output
- ✅ Written in **pure C**, no external libraries
- ✅ 32 general purpose registers
- ✅ 8KiB, 2048 word **unified** memory, for both instructions and data

## 🛠️ Pipeline Stages

| Stage | Description                   |
|-------|-------------------------------|
| IF    | Instruction Fetch             |
| ID    | Instruction Decode / Reg Fetch |
| EX    | Execute / ALU operations      |
| MEM   | Memory Access (LW, SW)        |
| WB    | Write-back to registers       |

## 📄 Supported Instructions

- Arithmetic: `ADD`, `SUB`, `MULI` , `ADDI`
- Logic: `ANDI`, `ORI`
- Memory: `LW`, `SW`
- Shift: `SLL`, `SRL`
- Branch: `BNE` , `J`

## 🚀 Getting Started

### Build & Run
in 'Milestone2/build' directory

```bash
cmake ..
make 
./CASimulator