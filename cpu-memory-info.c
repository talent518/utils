// gcc -Wno-unused-result -O3 -o cpu-memory-info cpu-memory-info.c -lm && ./cpu-memory-info

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>

#define LINES 20

#define cpu_mem_head(c, m) if(hasCpu) {printf(c);} if(hasCpu && hasMem) {printf("|");} if(hasMem) {printf(m);} printf("\n")

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
	static char strcpu[8];
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
	while(ptr && sscanf(ptr, "%[^:]: %ld", key, &val)) {
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

typedef struct {
	// process memory
	long int size; // total program size (same as VmSize in /proc/[pid]/status)
	long int resident; // resident set size (same as VmRSS in /proc/[pid]/status)
	long int share; // shared pages (i.e., backed by a file)
	long int text; // text (code)
	long int lib; // library (unused in Linux 2.6)
	long int data; // data + stack
	long int dirty; // dirty KB(unused in Linux 2.6)
	long int rssFile; // resident - dirty KB

	// process cpu
	unsigned int threads; // Number of threads in this process (since Linux 2.6)
	long int utime; // Amount  of  time that this process has been scheduled in user mode, measured in clock ticks
	long int stime; // Amount of time that this process has been scheduled in kernel mode, measured in clock ticks
	long int cutime; // Amount of time that this process's waited-for children have been scheduled in user mode, measured in clock ticks
	long int cstime; // Amount of time that this process's waited-for children have been scheduled in kernel mode, measured in clock ticks
} process_t;

//获取第N项开始的指针
const char* get_items(const char*buffer ,unsigned int item){
	const char *p =buffer;

	register int len = strlen(buffer);
	register int count = 0, i;

	for (i=0; i<len;i++){
		if (' ' == *p){
			count ++;
			if(count == item -1){
				p++;
				break;
			}
		}
		p++;
	}

	return p;
}

int getprocessinfo(int pid, process_t *proc) {
	static char buff[1024] = "";
	static char fname[64] = "";
	static char key[20] = "";
	static long int val = 0;
	FILE *fp;
	char *ptr;
	register int i;

	snprintf(fname, sizeof(fname), "/proc/%d/statm", pid);

	fp = fopen(fname, "r");
	if(!fp) {
		return 0;
	}

	memset(buff, 0, sizeof(buff));
	fgets(buff, sizeof(buff) - 1, fp);

	fclose(fp);

	sscanf(buff, "%ld%ld%ld%ld%ld%ld%ld", &proc->size, &proc->resident, &proc->share, &proc->text, &proc->lib, &proc->data, &proc->dirty);

	snprintf(fname, sizeof(fname), "/proc/%d/stat", pid);

	fp = fopen(fname, "r");
	if(!fp) {
		return 0;
	}

	memset(buff, 0, sizeof(buff));
	fgets(buff, sizeof(buff) - 1, fp);

	fclose(fp);

	const char *q = get_items(buff, 14);
	sscanf(q, "%ld%ld%ld%ld", &proc->utime, &proc->stime, &proc->cutime, &proc->cstime);
	q = get_items(q, 7);
	sscanf(q, "%ld", &proc->threads);

	snprintf(fname, sizeof(fname), "/proc/%d/status", pid);

	fp = fopen(fname, "r");
	fp = fopen(fname, "r");
	if(!fp) {
		return 0;
	}

	memset(buff, 0, sizeof(buff));
	fread(buff, sizeof(buff) - 1, 1, fp);

	fclose(fp);

	ptr = buff;
	while(ptr && sscanf(ptr, "%[^:]: %ld", key, &val)) {
		if(!strcmp(key, "RssFile")) {
			proc->rssFile = val;
			proc->dirty = proc->resident * 4 - val;
		}
		ptr = strchr(ptr, '\n');
		if(ptr) {
			ptr++;
		}
	}

	return 1;
}

int main(int argc, char *argv[]){
	cpu_t cpu, cpu2;
	mem_t mem;
	process_t proc, proc2;

	unsigned int all, all2, pall, pall2, i, delay = 1;

	double total;
	long int realUsed;
	char hasCpu = 1, hasMem = 1;
	int pid = 0;
	
	for(i=1; i<argc; i++) {
		switch(argv[i][0]) {
			case '-':
				switch(argv[i][1]) {
					case 'c':
						hasMem = 0;
						hasCpu = 1;
						break;
					case 'm':
						hasMem = 1;
						hasCpu = 0;
						break;
					case 'p':
						if(i+1 < argc) {
							hasCpu = 0;
							hasMem = 0;
							pid = atoi(argv[i+1]);
							break;
						}
					case 'h':
					case '?':
						printf("Usage: %s [ -c | -m | -p <pid> | -h | -? ] [ delay]\n"
								"  -c        Cpu info\n"
								"  -m        Memory info\n"
								"  -p <pid>  Process info\n"
								"  -h,-?     This help\n", argv[0]);
						return 0;
				}
				break;
			default:
				delay = atoi(argv[1]);
				if(delay <= 0) {
					delay = 1;
				}
				break;
		}
	}
	
	i = LINES;

	if(hasCpu) {
		getcpu(&cpu);

		all = cpu.user + cpu.nice + cpu.system + cpu.idle + cpu.iowait + cpu.irq + cpu.softirq + cpu.stolen + cpu.guest;
	}

	if(pid > 0) {
		if(!getprocessinfo(pid, &proc)) {
			printf("process not exists.\n");
			return 0;
		}

		pall = proc.utime + proc.stime + proc.cutime + proc.cstime;

		getcpu(&cpu);

		all = cpu.user + cpu.nice + cpu.system + cpu.idle + cpu.iowait + cpu.irq + cpu.softirq + cpu.stolen + cpu.guest;
	}

	while(1) {
		if(i++ % LINES == 0) {
			i = 1;
			
			if(hasCpu && hasMem) {
				cpu_mem_head("------------------------------------------------------------", "------------------------------------------------------|-------------------");
				cpu_mem_head("                           CPU (%%)                          ", "                      Memory Size                     |  Real Memory (%%)  ");
			}

			if(hasCpu || hasMem) {
				cpu_mem_head("------------------------------------------------------------", "------------------------------------------------------|-------------------");
				cpu_mem_head(" User  Nice System   Idle IOWait   IRQ SoftIRQ Stolen  Guest", "MemTotal  MemFree   Cached  Buffers SwapTotal SwapFree|Memory Cached  Swap");
				cpu_mem_head("------------------------------------------------------------", "------------------------------------------------------|-------------------");
			}

			if(pid > 0) {
				printf("-------------------------------------------------------------------------|--------|------------------------\n");
				printf("                          Memory Size                                    |        |         CPU (%%)       \n");
				printf("-------------------------------------------------------------------------|nThreads|------------------------\n");
				printf("    Size      RSS    Share     Text  Library Data+Stack    Dirty     Real|        |    User  Kernel   Total\n");
				printf("-------------------------------------------------------------------------|--------|------------------------\n");
			}
			fflush(stdout);
		}

		sleep(delay);

		if(hasCpu) {
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
			printf("%7.2f", (double)(cpu2.guest - cpu.guest) / total);
		
			cpu = cpu2;
			all = all2;
		}
		
		if(hasCpu && hasMem) {
			printf("|");
		}

		if(hasMem) {
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
			printf("%6.2f", mem.swapTotal ? (double)(mem.swapTotal - mem.swapFree) * 100.0 / (double)mem.swapTotal : 0.0); // SwapPercent
		}

		if(pid > 0) {
			if(!getprocessinfo(pid, &proc2)) {
				return 0;
			}

			getcpu(&cpu);

			all2 = cpu.user + cpu.nice + cpu.system + cpu.idle + cpu.iowait + cpu.irq + cpu.softirq + cpu.stolen + cpu.guest;
			total = (all2 - all) / 100.0;
			pall2 = proc2.utime + proc2.stime + proc2.cutime + proc2.cstime;

			printf("%8s", fsize(proc2.size * 4));
			printf("%9s", fsize(proc2.resident * 4));
			printf("%9s", fsize(proc2.share * 4));
			printf("%9s", fsize(proc2.text * 4));
			printf("%9s", fsize(proc2.lib * 4));
			printf("%11s", fsize(proc2.data * 4));
			printf("%9s", fsize(proc2.dirty));
			printf("%9s|", fsize(proc2.rssFile));
			printf("% 8d|", proc2.threads);

			double utime =  (double)(proc2.utime - proc.utime + proc2.cutime - proc.cutime) / total;
			double stime =  (double)(proc2.stime - proc.stime + proc2.cstime - proc.cstime) / total;
			printf("%8.2f", utime);
			printf("%8.2f", stime);
			printf("%8.2f", utime + stime);

			proc = proc2;
			all = all2;
		}
		
		printf("\n");
		
		fflush(stdout);
	}

	return 0;
}
