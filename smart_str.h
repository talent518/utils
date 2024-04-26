#ifndef _SMART_STR_H
#define _SMART_STR_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
	uint32_t rpos;
	uint32_t tpos;
	uint32_t size;
	uint8_t *buf;
} smart_str_t;

#define defSmartStr(var, size) \
	static uint8_t ss_##var##_buf[size]; \
	smart_str_t ss_##var = {0, 0, size, ss_##var##_buf}

#define extSmartStr(var) extern smart_str_t ss_##var

#define smart_str_put(var, buf, len) _smart_str_put(&ss_##var, (const uint8_t *) (buf), len)
#define smart_str_get(var, buf, len) _smart_str_get(&ss_##var, (uint8_t*) (buf), len)

bool _smart_str_put(smart_str_t *ss, const uint8_t *buf, uint32_t len);
uint32_t _smart_str_get(smart_str_t *ss, uint8_t *buf, uint32_t len);

#endif

