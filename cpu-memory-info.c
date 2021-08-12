// gcc -Wno-unused-result -O3 -o cpu-memory-info cpu-memory-info.c -lm && ./cpu-memory-info

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <dirent.h>
#include <signal.h>

#define LINES 20
#define NPROC 1000

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
	unsigned long int locked; // Mlocked
	unsigned long int swapTotal; // SwapTotal
	unsigned long int swapFree; // SwapFree
	unsigned long int shared; // Shmem
} mem_t;

void getmem(mem_t *mem) {
	static char buff[2048] = "";
	static char key[20] = "";
	static long int val = 0;
	FILE *fp;
	char *ptr;
	int i = 0377;
	
	memset(mem, 0, sizeof(mem_t));
	
	fp = fopen("/proc/meminfo", "r");

	memset(buff, 0, sizeof(buff));
	fread(buff, sizeof(buff) - 1, 1, fp);

	fclose(fp);
	
	ptr = buff;
	while(ptr && sscanf(ptr, "%[^:]: %ld", key, &val)) {
		// printf("%s => %d\n", key, val);

		if((i & 01) && !strcmp(key, "MemTotal")) {
			mem->total = val;
			i ^= 01;
			continue;
		}
		if((i & 02) && !strcmp(key, "MemFree")) {
			mem->free = val;
			i ^= 02;
			continue;
		}
		if((i & 04) && !strcmp(key, "Buffers")) {
			mem->buffers = val;
			i ^= 04;
			continue;
		}
		if((i & 010) && !strcmp(key, "Cached")) {
			mem->cached = val;
			i ^= 010;
			continue;
		}
		if((i & 020) && !strcmp(key, "Mlocked")) {
			mem->locked = val;
			i ^= 020;
			continue;
		}
		if((i & 040) && !strcmp(key, "SwapTotal")) {
			mem->swapTotal = val;
			i ^= 040;
			continue;
		}
		if((i & 0100) && !strcmp(key, "SwapFree")) {
			mem->swapFree = val;
			i ^= 0100;
			continue;
		}
		if((i & 0200) && !strcmp(key, "Shmem")) {
			mem->shared = val;
			i ^= 0200;
			continue;
		}

		ptr = strchr(ptr, '\n');
		if(ptr) {
			ptr++;
		}
	}
}

char *fsize(unsigned long int size) {
	static char buf[32];
	static char units[5] = "KMGT";
	unsigned int unit;

	if(!size) {
		return "0K";
	}
	
	unit = (int)(log(size)/log(1024));
	if (unit > 3) {
		unit=3;
	}

	sprintf(buf, "%.2lf%c", (double)size/pow(1024,unit), units[unit]);

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
	
	unsigned long int etime; // runned time for seconds
} process_t;

//获取第N项开始的指针
const char* get_items(const char*buffer, unsigned int item) {
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

unsigned int getprocessdirtys(int pid) {
	static char buff[16*1024*1024] = "";
	static char fname[64] = "";
	static char key[64] = "";
	static long int val = 0;
	FILE *fp;
	char *ptr;
	
	snprintf(fname, sizeof(fname), "/proc/%d/smaps", pid);

	fp = fopen(fname, "r");
	if(!fp) {
		return 0;
	}

	memset(buff, 0, sizeof(buff));
	fread(buff, sizeof(buff) - 1, 1, fp);

	fclose(fp);
	
	ptr = buff;
	
	unsigned int dirtys = 0;
	while(ptr && sscanf(ptr, "%[^:]: %ld", key, &val)) {
		if(!strcmp(key, "Private_Dirty") || !strcmp(key, "Shared_Dirty")) {
			dirtys += val;
		}
		ptr = strchr(ptr, '\n');
		if(ptr) {
			ptr++;
		}
	}
	
	return dirtys;
}

int getprocessinfo(int pid, process_t *proc) {
	static char buff[1024] = "";
	static char fname[64] = "";
	static char key[20] = "";
	static long int val = 0;
	FILE *fp;
	char *ptr;

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
	sscanf(q, "%u", &proc->threads);
	q = get_items(q, 3);
	
	unsigned long int etime = 0;
	sscanf(q, "%lu", &etime);
	
	fp = fopen("/proc/uptime", "r");
	fgets(buff, sizeof(buff) - 1, fp);

	fclose(fp);
	
	unsigned long int uptime = 0;
	sscanf(buff, "%lu", &uptime);
	
	int tck = sysconf(_SC_CLK_TCK);
	proc->etime = uptime - etime / tck;

	snprintf(fname, sizeof(fname), "/proc/%d/status", pid);

	fp = fopen(fname, "r");
	if(!fp) {
		return 0;
	}

	memset(buff, 0, sizeof(buff));
	fread(buff, sizeof(buff) - 1, 1, fp);

	fclose(fp);

	ptr = buff;
	proc->dirty = 0;
	proc->rssFile = 0;
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
	
	if(!proc->dirty || !proc->rssFile) {
		proc->dirty = getprocessdirtys(pid);
		proc->rssFile  = proc->resident * 4 - proc->dirty;
	}

	return 1;
}

int getcomm(char *pid, char *comm) {
	static char buff[128] = "";
	static char fname[267] = "";
	FILE *fp;
	char *ptr;
	int len;

	snprintf(fname, sizeof(fname), "/proc/%s/comm", pid);

	fp = fopen(fname, "r");
	if(!fp) {
		return 0;
	}

	memset(buff, 0, sizeof(buff));
	len = fread(buff, 1, sizeof(buff) - 1, fp);

	fclose(fp);
	
	if(len <= 0) {
		return 0;
	}
	
	ptr = buff + len - 1;
	while(buff <= ptr && (*ptr == '\r' || *ptr == '\n' || *ptr == ' ')) {
		*ptr-- = '\0';
	}
	
	return !strcmp(buff, comm);
}

char procArgStr[NPROC][1024];

int procarg(char *comm, int nproc, int *pid, process_t *proc, unsigned int *pall) {
	static char fname[64] = "";
	int n, len, i;
	
	if(comm) {
		DIR *dp;
		struct dirent *dt;
		register char *p;
	
		nproc = 0;
		dp = opendir("/proc");
		while((dt = readdir(dp)) != NULL) {
			p = dt->d_name;
			while(*p && *p >= '0' && *p <= '9') p++;
			if(*p == '\0') {
				if(getcomm(dt->d_name, comm)) {
					pid[nproc++] = atoi(dt->d_name);
					if(nproc >= NPROC) break;
				}
			}
		}
		closedir(dp);
	}
	
	for(n=0; n<nproc; n++) {
		int len = snprintf(fname, sizeof(fname), "/proc/%d/cmdline", pid[n]);
		FILE *fp = fopen(fname, "r");
		if(!fp) {
			pid[n] = 0;
			continue;
		}
		len = fread(procArgStr[n], 1, sizeof(procArgStr[0]), fp);
		fclose(fp);

		procArgStr[n][len-1] = '\0';

		for(i=0; i<len-1; i++) {
			if(procArgStr[n][i] == '\0') {
				procArgStr[n][i] = ' ';
			}
		}

		if(!getprocessinfo(pid[n], &proc[n])) {
			pid[n] = 0;
			continue;
		}

		pall[n] = proc[n].utime + proc[n].stime + proc[n].cutime + proc[n].cstime;
	}
	
	return nproc;
}

static int lines = 0;

static void signal_handler(int sig) {
	lines = 0;
	printf("\r");
}

int main(int argc, char *argv[]){
	cpu_t cpu, cpu2;
	mem_t mem;
	int pid[NPROC];
	process_t proc[NPROC], proc2[NPROC];

	unsigned int all, all2, pall[NPROC], pall2[NPROC], i, delay = 1;

	double total;
	long int realUsed;
	char hasCpu = 1, hasMem = 1;
	int nproc = 0, n;
	char *comm = NULL;
	
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
					case 'P':
						if(i+1 < argc && nproc == 0) {
							hasCpu = 0;
							hasMem = 0;
							comm = argv[i+1];
							i++;
							break;
						}
					case 'p':
						if(i+1 < argc && comm == NULL) {
							hasCpu = 0;
							hasMem = 0;
							if(nproc<NPROC) {
								pid[nproc++] = atoi(argv[i+1]);
							}
							i++;
							break;
						}
					default:
						printf("Usage: %s [ -c | -m | -P <comm> | -p <pid> | -h | -? ] [ delay]\n"
								"  -c        Cpu info\n"
								"  -m        Memory info\n"
								"  -P <comm>  Process comm\n"
								"  -p <pid>  Process info(Multiple)\n"
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

	nproc = procarg(comm, nproc, pid, proc, pall);

	if(nproc>0 || comm) {
		getcpu(&cpu);
		all = cpu.user + cpu.nice + cpu.system + cpu.idle + cpu.iowait + cpu.irq + cpu.softirq + cpu.stolen + cpu.guest;
	}

	signal(SIGQUIT, signal_handler);

	int nn;
	while(1) {
		if(lines++ % LINES == 0) {
			if(hasCpu && hasMem) {
				cpu_mem_head("------------------------------------------------------------", "------------------------------------------------------|--------------------");
				cpu_mem_head("                           CPU (%%)                          ", "                      Memory Size                     |   Real Memory (%%)  ");
			}

			if(hasCpu || hasMem) {
				cpu_mem_head("------------------------------------------------------------", "------------------------------------------------------|--------------------");
				cpu_mem_head(" User  Nice System   Idle IOWait   IRQ SoftIRQ Stolen  Guest", "MemTotal  MemFree   Cached  Buffers SwapTotal SwapFree|Memory Cached   Swap");
				cpu_mem_head("------------------------------------------------------------", "------------------------------------------------------|--------------------");
			}
			
			nproc = procarg(comm, nproc, pid, proc, pall);

			if(nproc > 0) {
				printf("--------------------------------------------------------------------------------------------------------------------\n");
				nn = nproc;
				for(n=0; n<nproc; n++) {
					if(pid[n]>0) {
						unsigned int mtime = proc[n].etime/60;
						unsigned int htime = mtime/60;
						unsigned int dtime = htime/24;
						printf("%d:\n  Run Time:(%lu seconds) ", pid[n], proc[n].etime);
						if(dtime>0) {
							printf("%d-", dtime);
						}
						printf("%02d:%02d:%02ld\n", htime%24, mtime%60, proc[n].etime%60);
						printf("  Command: %s\n", procArgStr[n]);
					} else {
						nn --;
					}
				}
				if(nn<=0 && comm == NULL) {
					return 0;
				}
				printf("--------------------------------------------------------------------------------------------------------------------\n");
				printf("        |                          Memory Size                                    |        |         CPU (%%)       \n");
				printf("   PID  |-------------------------------------------------------------------------|nThreads|------------------------\n");
				printf("        |    Size      RSS    Share     Text  Library Data+Stack    Dirty     Real|        |    User  Kernel   Total\n");
				printf("--------|-------------------------------------------------------------------------|--------|------------------------\n");
			}
			fflush(stdout);
			lines = 1;
		}

		sleep(delay);

		if(hasCpu) {
			getcpu(&cpu2);

			all2 = cpu2.user + cpu2.nice + cpu2.system + cpu2.idle + cpu2.iowait + cpu2.irq + cpu2.softirq + cpu2.stolen + cpu2.guest;

			total = (all2 - all) / 100.0;
			if(total == 0) total = 1;
		
			printf("%5.2f", (float)((double)(cpu2.user - cpu.user) / total));
			printf("%6.2f", (float)((double)(cpu2.nice - cpu.nice) / total));
			printf("%7.2f", (float)((double)(cpu2.system - cpu.system) / total));
			printf("%7.2f", (float)((double)(cpu2.idle - cpu.idle) / total));
			printf("%7.2f", (float)((double)(cpu2.iowait - cpu.iowait) / total));
			printf("%6.2f", (float)((double)(cpu2.irq - cpu.irq) / total));
			printf("%8.2f", (float)((double)(cpu2.softirq - cpu.softirq) / total));
			printf("%7.2f", (float)((double)(cpu2.stolen - cpu.stolen) / total));
			printf("%7.2f", (float)((double)(cpu2.guest - cpu.guest) / total));
		
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
			//printf("%6.2f", (float)((double)(mem.total - mem.free) * 100.0 / (double)mem.total)); // MemPercent
			printf("%6.2f", (float)((double)(realUsed = mem.total - mem.free - mem.cached - mem.buffers) * 100.0 / (double)mem.total)); // MemRealPercent
			printf("%7.2f", (float)((double)(mem.cached) * 100.0 / (double)mem.total)); // MemCachedPercent
			printf("%7.2f", (float)(mem.swapTotal ? (double)(mem.swapTotal - mem.swapFree) * 100.0 / (double)mem.swapTotal : 0.0)); // SwapPercent
		}

		if(hasCpu || hasMem) {
			printf("\nPress Ctrl+\\ key for show table head\r");
			continue;
		}

		if(nproc>0) {
			getcpu(&cpu);

			all2 = cpu.user + cpu.nice + cpu.system + cpu.idle + cpu.iowait + cpu.irq + cpu.softirq + cpu.stolen + cpu.guest;
			total = (all2 - all) / 100.0;
			if(total == 0) total = 1;
			all = all2;
		}

		nn = nproc;
		for(n=0; n<nproc; n++) {
			if(pid[n] ==0 || !getprocessinfo(pid[n], &proc2[n])) {
				pid[n] = 0;
				nn--;
				continue;
			}

			pall2[n] = proc2[n].utime + proc2[n].stime + proc2[n].cutime + proc2[n].cstime;

			printf("%8d|", pid[n]);
			printf("%8s", fsize(proc2[n].size * 4));
			printf("%9s", fsize(proc2[n].resident * 4));
			printf("%9s", fsize(proc2[n].share * 4));
			printf("%9s", fsize(proc2[n].text * 4));
			printf("%9s", fsize(proc2[n].lib * 4));
			printf("%11s", fsize(proc2[n].data * 4));
			printf("%9s", fsize(proc2[n].dirty));
			printf("%9s|", fsize(proc2[n].rssFile));
			printf("%8d|", proc2[n].threads);

			double utime =  (double)(proc2[n].utime - proc[n].utime + proc2[n].cutime - proc[n].cutime) / total;
			double stime =  (double)(proc2[n].stime - proc[n].stime + proc2[n].cstime - proc[n].cstime) / total;
			printf("%8.2f", (float) utime);
			printf("%8.2f", (float) stime);
			printf("%8.2f\n", (float)(utime + stime));

			proc[n] = proc2[n];
		}
		if(nproc > 0 && nn <= 0 && comm == NULL) {
			return 0;
		} else if(nn > 1) {
			printf("--------------------------------------------------------------------------------------------------------------------\n");
		} else if(nn == 0) {
			lines = 0;
			continue;
		}
		printf("Press Ctrl+\\ key for show table head\r");
		
		fflush(stdout);
	}

	return 0;
}
