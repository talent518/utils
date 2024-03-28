#include <string.h>

#include "smart_str.h"

bool _smart_str_put(smart_str_t *ss, const uint8_t *buf, uint16_t len) {
	uint16_t i, remain, sz;

	if(!len)
		return false;

	if(ss->rpos > ss->tpos) {
		remain = ss->rpos - ss->tpos;
	} else {
		remain = ss->size - ss->tpos + ss->rpos;
	}

	if(remain <= len)
		return false;

	sz = ss->size - ss->tpos;
	if(sz < len) {
		memcpy(ss->buf + ss->tpos, buf, sz);
		memcpy(ss->buf, buf + sz, len - sz);
		ss->tpos = len - sz;
	} else {
		memcpy(ss->buf + ss->tpos, buf, len);

		i = ss->tpos + len;
		if(i == ss->size) i = 0;
		ss->tpos = i;
	}

	return true;
}

uint16_t _smart_str_get(smart_str_t *ss, uint8_t *buf, uint16_t len) {
	uint16_t i, written, sz;
	if(!len)
		return 0;

	if(ss->rpos == ss->tpos)
		return 0;

	if(ss->tpos > ss->rpos) {
		written = ss->tpos - ss->rpos;
	} else {
		written = ss->size - ss->rpos + ss->tpos;
	}

	if(len > written)
		len = written;

	sz = ss->size - ss->rpos;
	if(sz < len) {
		memcpy(buf, ss->buf + ss->rpos, sz);
		memcpy(buf + sz, ss->buf, len - sz);
		ss->rpos = len - sz;
	} else {
		memcpy(buf, ss->buf + ss->rpos, len);

		i = ss->rpos + len;
		if(i == ss->size) i = 0;
		ss->rpos = i;
	}

	return len;
}
