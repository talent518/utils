#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <sys/random.h>
#include <semaphore.h>

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

#define szHDR 4
#define SIZE 512
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

void sighandler(int sig) {
	is_running = false;
}

void *strbuf_read(void *arg) {
	strbuf_t *buf = (strbuf_t*) malloc(szHDR + SIZE + 1);
	uint16_t sz = 0;
	uint64_t n = 0;
	bool is_ok = true;
	
	while(is_running) {
		sz = smart_str_get(strbuf, buf, szHDR);
		if(sz == 0) {
			continue;
		} else if(sz < szHDR || buf->size > SIZE) {
			is_ok = false;
			printf("\nHEADER ERROR\n  size: %u\n  crc: %04X\n  sz: %u\n", buf->size, buf->crc, sz);
			break;
		} else {
			sz = smart_str_get(strbuf, buf->data, buf->size);
			if(sz == buf->size) {
				buf->data[buf->size] = 0;
				if(buf->crc == crc16(vCRC, (const uint8_t *) buf->data, buf->size)) {
					printf("%016lX\r", ++n);
				} else {
					is_ok = false;
					printf("\nCRC ERROR\n  size: %u\n  crc: %04X\n  data: %s\n", buf->size, buf->crc, buf->data);
					break;
				}
			} else {
				buf->data[sz] = 0;
				is_ok = false;
				printf("\nDATA ERROR\n  size: %u\n  crc: %04X\n  data: %s\n", buf->size, buf->crc, buf->data);
				break;
			}
		}
	}
	
	free(buf);
	
	if(is_ok) printf("\n");
	else is_running = false;
	
	pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
	pthread_t thread;
	pthread_attr_t attr;
	strbuf_t *buf;
	uint16_t i,size;
	uint8_t sizes[SIZE+2];
	
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	if(pthread_create(&thread, &attr, strbuf_read, NULL)) {
		fprintf(stderr, "pthread_create: %s\n", strerror(errno));
		return 1;
	}
	pthread_attr_destroy(&attr);
	
	signal(SIGINT, sighandler);
	
	buf = (strbuf_t*) malloc(szHDR + SIZE);
	
	while(is_running) {
		if(getrandom(sizes, sizeof(sizes), GRND_RANDOM) <= 2) {
			fprintf(stderr, "\ngetrandom: %s\n", strerror(errno));
			continue;
		}
		
		buf->size = ((* (uint16_t*) &sizes[0]) % SIZE + 1);
		for(i = 0; i < buf->size; i++) {
			buf->data[i] = strmap[sizes[2+i]&0x3f];
		}
		buf->crc = crc16(vCRC, (const uint8_t *) buf->data, buf->size);
		
		while(is_running && !smart_str_put(strbuf, buf, szHDR + buf->size));
	}
	
	free(buf);

	pthread_join(thread, NULL);
	
	return 0;
}

