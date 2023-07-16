#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <sys/random.h>
#include <semaphore.h>
#include <time.h>
#include <sys/time.h>

#define CRC16_NO_MAIN
#include "crc16.c"

#include "smart_str.h"

typedef struct {
	uint16_t size;
	uint16_t crc;
	uint8_t data[1];
} strbuf_t;

defSmartStr(strbuf,2048);

volatile bool is_running = true;
static uint16_t SIZE = 10;
static sem_t sem;

#define szHDR 4
#define vCRC 0xffff

static const uint8_t strmap[64]={
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J',
    'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T',
    'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd',
    'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
    'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x',
    'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7',
    '8', '9', '+', '/' 
};

double microtime() {
    struct timeval tv = {0, 0};

    if(gettimeofday(&tv, NULL)) tv.tv_sec = time(NULL);

    return (double) tv.tv_sec + tv.tv_usec / 1000000.0f;
}

void sighandler(int sig) {
	is_running = false;
}

void *strbuf_read(void *arg) {
	strbuf_t *buf = (strbuf_t*) malloc(szHDR + SIZE + 1);
	uint16_t sz = 0;
	uint32_t n = 0;
	double t = microtime(), t2;
	
	while(is_running) {
		sem_wait(&sem);
		sz = smart_str_get(strbuf, buf, szHDR);
		if(sz == 0) {
			break;
		} else if(sz < szHDR || buf->size > SIZE) {
			printf("HEADER ERROR\n  size: %u\n  crc: %04X\n  sz: %u\n", buf->size, buf->crc, sz);
			break;
		} else {
			sz = smart_str_get(strbuf, buf->data, buf->size);
			if(sz == buf->size) {
				buf->data[buf->size] = 0;
				if(buf->crc == crc16(vCRC, (const uint8_t *) buf->data, buf->size)) {
					n++;
					
					if(n % 10000 == 0) {
						t2 = (microtime() - t);
						printf("%u %.3lf %.3lf\n", n, n / t2, t2);
					}
				} else {
					printf("CRC ERROR\n  size: %u\n  crc: %04X\n  data: %s\n", buf->size, buf->crc, buf->data);
					break;
				}
			} else {
				buf->data[sz] = 0;
				printf("DATA ERROR\n  size: %u\n  crc: %04X\n  data: %s\n", buf->size, buf->crc, buf->data);
				break;
			}
		}
	}
	
	free(buf);
	
	is_running = false;
	
	if(n % 10000) {
		t2 = (microtime() - t);
		printf("%u %.3lf %.3lf\n", n, n / t2, t2);
	}
	
	pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
	pthread_t thread;
	pthread_attr_t attr;
	strbuf_t *buf;
	uint16_t i,size;
	uint32_t times = (argc < 2 ? 100000 : strtoul(argv[1], NULL, 10));
	uint8_t *sizes;
	
	if(argc > 2) {
		SIZE = strtoul(argv[2], NULL, 10);
		if(SIZE < 10) SIZE = 10;
		else if(SIZE + szHDR > ss_strbuf.size) SIZE = ss_strbuf.size - szHDR;
	}
	
	sem_init(&sem, 0, 0);
	
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	if(pthread_create(&thread, &attr, strbuf_read, NULL)) {
		fprintf(stderr, "pthread_create: %s\n", strerror(errno));
		return 1;
	}
	pthread_attr_destroy(&attr);
	
	signal(SIGINT, sighandler);
	
	sizes = (uint8_t*) malloc(SIZE + 2);
	buf = (strbuf_t*) malloc(szHDR + SIZE);
	
	while(is_running && times) {
		if(getrandom(sizes, SIZE + 2, GRND_RANDOM) <= 2) {
			fprintf(stderr, "getrandom: %s\n", strerror(errno));
			continue;
		}
		
		buf->size = ((* (uint16_t*) &sizes[0]) % SIZE + 1);
		for(i = 0; i < buf->size; i++) {
			buf->data[i] = strmap[sizes[2+i]&0x3f];
		}
		buf->crc = crc16(vCRC, (const uint8_t *) buf->data, buf->size);
		
		while(is_running && !smart_str_put(strbuf, buf, szHDR + buf->size));
		
		sem_post(&sem);
		times --;
	}
	
	free(buf);
	free(sizes);

	sem_post(&sem);
	pthread_join(thread, NULL);
	
	return 0;
}

