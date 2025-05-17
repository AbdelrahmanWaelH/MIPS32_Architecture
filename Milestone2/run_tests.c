#include <stdio.h>

extern char* filepath;
extern void runPipeline();
extern void readFileToMemory(char* filepath);
extern void parseTextInstruction();
extern void printMainMemory();

void runTest(const char* testFile, const char* description) {
    printf("\n========================================\n");
    printf("Running test: %s\n", testFile);
    printf("Description: %s\n", description);
    printf("========================================\n");
    
    filepath = (char*)testFile;
    readFileToMemory(filepath);
    parseTextInstruction();
    printf("\nStarting pipeline execution for %s...\n", testFile);
    runPipeline();
}

// int main() {
//     // Test data hazards
//     runTest("test_data_hazards.txt", "Testing data hazards with forwarding");
//
//     // Test control hazards
//     runTest("test_control_hazard.txt", "Testing branch prediction and flushing");
//
//     // Test combined hazards
//     runTest("test_combined_hazards.txt", "Testing combination of data and control hazards");
//
//     return 0;
// }
