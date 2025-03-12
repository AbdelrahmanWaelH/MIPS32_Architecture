#include <stdio.h>
int main() {
	char filename[] = "test.txt";
	FILE *file = fopen(filename, "r");
	if (file == NULL) {
		printf("Error in opening file: %s", filename);
		return 1;
	}
	char c;
	while ((c = fgetc(file)) != EOF) {
		printf("%c", c);
	}
	fclose(file);
	return 0;
}
