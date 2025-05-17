#include <stdio.h>
#include <stdlib.h>
#include "FileReader.h"

char* readFile(char* filepath) {
	
	FILE *file = fopen(filepath, "r");

	char* outputBuffer = (char*)malloc(16*4096);

	if (file == NULL) {
		printf("Error in opening file: %s", filepath);
		return NULL;
	}

	char c;
	int bufferIndex = 0;
	while ((c = fgetc(file)) != EOF) {
		//printf("%c", c);
		outputBuffer[bufferIndex] = c;
		bufferIndex++;
	}

	outputBuffer[bufferIndex] = '\0';
	fclose(file);
	return outputBuffer;
}
