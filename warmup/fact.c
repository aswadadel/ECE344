#include "common.h"

int fact(int output, int i);

int main(int argc, char **argv)
{
	if(argc < 2) {
		printf("Huh?\n");
		return 0;
	}
	int input = 0;
	char *s = argv[1];
	char c = s[0];
	for(int i = 0; c != '\0'; c = s[++i]){
		int num = c-'0';
		if(num < 0 || num > 9){
			printf("Huh?\n");
			return 0;
		} else {
			input = input*10 + num;
		}
	}
	if(input == 0){
		printf("Huh?\n");
		return 0;
	}
	if(input > 12){
		printf("Overflow\n");
		return 0;
	}
	int output = fact(1,input);
	printf("%d\n", output);
	return 0;
}

int fact(int output, int i){
	if(i == 1){
		return output;
	} else {
		output = i*output;
		i--;
		return fact(output, i);
	}
}
