#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define strSequence "01234567"
#define lenSequence sizeof(strSequence) - 1

void seq(const char buf[], int b, int n) {
	static int id = 0;
	char str[lenSequence + 1];
	register char chr;
	register int i;
	
	memcpy(str, buf, n);
	str[n] = '\0';

	for(i=b; i<n; i++) {
		chr = str[b];
		str[b] = str[i];
		str[i] = chr;
		
		if(b == n-2) {
			printf("%10d: %s\n", ++id, str);
		}
		
		seq(str, b+1, n);
	}
}

int main(int argc, char *argv[]) {
	seq(strSequence, 0, lenSequence);

	return 0;
}
