#include "smart_str.h"

bool _smart_str_put(smart_str_t *ss, const uint8_t *buf, uint16_t len) {
	int i, j, n;

	if(!len) return false;

	n = ss->tpos - ss->rpos;
	if(n < 0) n += ss->size;

	if(n + len > ss->size) return false;

	for(i = ss->tpos, j = 0; j < len; j++)
	{
		ss->buf[i++] = buf[j];
		if(i >= ss->size) i = 0;
	}

	ss->tpos = i;
	
	return true;
}

uint16_t _smart_str_get(smart_str_t *ss, uint8_t *buf, uint16_t len) {
	int i, j, n;
	if(!len) return 0;

	if(ss->rpos == ss->tpos) return 0;

	n = ss->tpos - ss->rpos;
	if(n < 0) n += ss->size;

	if(len > n) len = n;

	for(i = ss->rpos, j = 0; j < len; j++)
	{
		buf[j] = ss->buf[i++];
		if(i >= ss->size) i = 0;
	}

	ss->rpos = i;

	return len;
}

