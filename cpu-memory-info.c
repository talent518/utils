// gcc -O3 -o cpu-memory-info cpu-memory-info.c -lm

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define LINES 20

typedef struct {
	unsigned long int user;
	unsigned long int nice;
	unsigned long int system;
	unsigned long int idle;
	unsigned long int iowait;
	unsigned long int irq;
	unsigned long int softirq;
	unsigned long int stolen;
	unsigned long int guest;
} cpu_t;

void getcpu(cpu_t *cpu) {
	static char buff[128];
	static strcpu[8];
	FILE *fp;
	
	memset(cpu, 0, sizeof(cpu_t));

	fp = fopen("/proc/stat", "r");

	memset(buff, 0, sizeof(buff));
	fgets(buff, sizeof(buff) - 1, fp);

	fclose(fp);
	
	sscanf(buff, "%s%ld%ld%ld%ld%ld%ld%ld%ld%ld", strcpu, &cpu->user, &cpu->nice, &cpu->system, &cpu->idle, &cpu->iowait, &cpu->irq, &cpu->softirq, &cpu->stolen, &cpu->guest);
}

typedef struct {
	unsigned long int total; // MemTotal
	unsigned long int free; // MemFree
	unsigned long int cached; // Cached
	unsigned long int buffers; // Buffers
	unsigned long int swapTotal; // SwapTotal
	unsigned long int swapFree; // SwapFree
} mem_t;

void getmem(mem_t *mem) {
	static char buff[2048] = "";
	static char key[20] = "";
	static long int val = 0;
	FILE *fp;
	char *ptr;
	int i = 077;
	
	memset(mem, 0, sizeof(mem_t));
	
	fp = fopen("/proc/meminfo", "r");

	memset(buff, 0, sizeof(buff));
	fread(buff, sizeof(buff) - 1, 1, fp);

	fclose(fp);
	
	ptr = buff;
	while(ptr && sscanf(ptr, "%[^:]: %d", key, &val)) {
		// printf("%s => %d\n", key, val);
		
		if(i & 01 && !strcmp(key, "MemTotal")) {
			mem->total = val;
			i ^= 01;
			continue;
		}
		if(i & 02 && !strcmp(key, "MemFree")) {
			mem->free = val;
			i ^= 02;
			continue;
		}
		if(i & 04 && !strcmp(key, "Buffers")) {
			mem->buffers = val;
			i ^= 04;
			continue;
		}
		if(i & 010 && !strcmp(key, "Cached")) {
			mem->cached = val;
			i ^= 010;
			continue;
		}
		if(i & 020 && !strcmp(key, "SwapTotal")) {
			mem->swapTotal = val;
			i ^= 020;
			continue;
		}
		if(i & 040 && !strcmp(key, "SwapFree")) {
			mem->swapFree = val;
			i ^= 040;
			continue;
		}
		
		ptr = strchr(ptr, '\n');
		if(ptr) {
			ptr++;
		}
	}
}

char *fsize(unsigned long int size) {
	static char buf[20];
	static char units[5] = "KMGT";
	unsigned int unit;

	if(!size) {
		return "0K";
	}
	
	unit = (int)(log(size)/log(1024));
	if (unit > 3) {
		unit=3;
	}

	sprintf(buf, "%.2f%c", size/pow(1024,unit), units[unit]);

	return buf;
}

int main(int argc, char *argv[]){
	cpu_t cpu, cpu2;
	mem_t mem;

	char buff[128] = {'\0'};
	char strcpu[6];

	int all, all2, i = LINES, delay = 1;

	double total;
	long int realUsed;
	
	if(argc == 2) {
		delay = atoi(argv[1]);
		if(delay <= 0) {
			delay = 1;
		}
	}

	getcpu(&cpu);

	all = cpu.user + cpu.nice + cpu.system + cpu.idle + cpu.iowait + cpu.irq + cpu.softirq + cpu.stolen + cpu.guest;

	while(1) {
		if(i++ % LINES == 0) {
			i = 1;
			
			printf("------------------------------------------------------------|------------------------------------------------------|-------------------\n");
			printf("                           CPU (%)                          |                      Memory Size                     |  Real Memory (%)  \n");
			printf("------------------------------------------------------------|------------------------------------------------------|-------------------\n");
			printf(" User  Nice System   Idle IOWait   IRQ SoftIRQ Stolen  Guest|MemTotal  MemFree   Cached  Buffers SwapTotal SwapFree|Memory Cached  Swap\n");
			printf("------------------------------------------------------------|------------------------------------------------------|-------------------\n");
			fflush(stdout);
		}

		sleep(delay);

		getcpu(&cpu2);

		all2 = cpu2.user + cpu2.nice + cpu2.system + cpu2.idle + cpu2.iowait + cpu2.irq + cpu2.softirq + cpu2.stolen + cpu2.guest;

		total = (all2 - all) / 100.0;
		
		printf("%5.2f", (double)(cpu2.user - cpu.user) / total);
		printf("%6.2f", (double)(cpu2.nice - cpu.nice) / total);
		printf("%7.2f", (double)(cpu2.system - cpu.system) / total);
		printf("%7.2f", (double)(cpu2.idle - cpu.idle) / total);
		printf("%7.2f", (double)(cpu2.iowait - cpu.iowait) / total);
		printf("%6.2f", (double)(cpu2.irq - cpu.irq) / total);
		printf("%8.2f", (double)(cpu2.softirq - cpu.softirq) / total);
		printf("%7.2f", (double)(cpu2.stolen - cpu.stolen) / total);
		printf("%7.2f|", (double)(cpu2.guest - cpu.guest) / total);

		getmem(&mem);
		printf("%8s", fsize(mem.total));
		printf("%9s", fsize(mem.free));
		printf("%9s", fsize(mem.cached));
		printf("%9s", fsize(mem.buffers));
		printf("%10s", fsize(mem.swapTotal));
		printf("%9s|", fsize(mem.swapFree));
		//printf("%6.2f", (double)(mem.total - mem.free) * 100.0 / (double)mem.total); // MemPercent
		printf("%6.2f", (double)(realUsed = mem.total - mem.free - mem.cached - mem.buffers) * 100.0 / (double)mem.total); // MemRealPercent
		printf("%7.2f", (double)(mem.cached) * 100.0 / (double)mem.total); // MemCachedPercent
		printf("%6.2f\n", mem.swapTotal ? (double)(mem.swapTotal - mem.swapFree) * 100.0 / (double)mem.swapTotal : 0.0); // SwapPercent
		
		fflush(stdout);
		
		cpu = cpu2; // memcpy(&cpu, &cpu2, sizeof(cpu_t));
		all = all2;
	}

    return 0;
}
