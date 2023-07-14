#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <sys/random.h>

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
volatile bool is_will_exit = false;

#define szHDR 4
#define SIZE 512
#define vCRC 0x1234

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
	uint32_t n = 0;
	
	while(is_running) {
		sz = smart_str_get(strbuf, buf, szHDR);
		if(sz == 0) {
			if(is_will_exit) break;
		} else if(sz < szHDR || buf->size > SIZE) {
			is_running = false;
			printf("\nHEADER ERROR\n");
		} else {
			sz = smart_str_get(strbuf, buf->data, buf->size);
			if(sz == buf->size) {
				if(buf->crc == crc16(vCRC, (const uint8_t *) buf->data, buf->size)) {
					printf("%u\r", ++n);
				} else {
					is_running = false;
					buf->data[buf->size] = 0;
					printf("\nCRC ERROR\n  size: %u\n  crc: %04X\n  data: %s\n", buf->size, buf->crc, buf->data);
				}
			} else {
				is_running = false;
				printf("\nDATA ERROR\n");
			}
		}
	}
	
	free(buf);
	
	pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
	pthread_t thread;
	pthread_attr_t attr;
	strbuf_t *buf;
	uint16_t i,size;
	uint32_t times = (argc < 2 ? 10000 : strtoul(argv[1], NULL, 10));
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
	
	while(is_running && times) {
		if(getrandom(sizes, sizeof(sizes), GRND_RANDOM) <= 2) {
			fprintf(stderr, "\ngetrandom: %s\n", strerror(errno));
			continue;
		}
		
		buf->size = ((* (uint16_t*) &sizes[0]) % SIZE + 1);
		for(i = 0; i < buf->size; i++) {
			buf->data[i] = strmap[sizes[2+i]&0x3f];
		}
		buf->crc = crc16(vCRC, (const uint8_t *) buf->data, buf->size);
		
		while(is_running && !smart_str_put(strbuf, buf, szHDR + buf->size)) usleep(10);
		
		times--;
	}
	
	free(buf);

	is_will_exit = !times;
	pthread_join(thread, NULL);
	if(is_running) printf("\n");
	
	return 0;
}

