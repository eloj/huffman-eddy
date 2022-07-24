#include <stdio.h>

int main(void) {

	for (int i=0 ; i < 256 ; ++i) {
		for (int x=0 ; x < i + 1 ; ++x) {
			printf("%c", i);
		}
	}

	return 0;
}
