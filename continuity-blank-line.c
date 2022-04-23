#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
	int i, j, line, linec, lines[10000], blank;
	FILE *fp;

	for(i = 1; i < argc; i ++) {
		fp = fopen(argv[i], "r");
		if(fp) {
			line = 1;
			blank = 1;
			linec = 0;
			while(!feof(fp)) {
				switch(fgetc(fp)) {
					case ' ':
					case '\t':
					case '\r':
						break;
					case '\n':
						if(blank) {
							lines[linec++] = line;
						} else {
							blank = 1;
						}
						line ++;
						break;
					default:
						blank = 0;
						break;
				}
			}
			fclose(fp);

			if(blank) lines[linec++] = line;

			blank = 0;
			for(j = 1; j < linec; j ++) {
				if(lines[j] == lines[j-1] + 1) {
					blank = 1;
					break;
				}
			}

			if(blank) {
				printf("%s", argv[i]);
				for(j = 1; j < linec; j ++) {
					if(lines[j] == lines[j-1] + 1) printf(" %d", lines[j]);
				}
				printf("\n");
			}
		}
	}

	return 0;
}

