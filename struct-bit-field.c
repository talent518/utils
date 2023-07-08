#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

typedef uint8_t u8;
typedef uint16_t u16;

// -Wno-packed-bitfield-compat
#define __packed __attribute__((packed))

typedef union {
	struct {
		u8 b0:1;
		u8 b1:1;
		u8 b2:1;
		u8 b3:1;
		u8 b4:1;
		u8 b5:1;
		u8 b6:1;
		u8 b7:1;
		u8 b8:1;
		u8 b9:1;
		u8 b10:1;
		u8 b11:1;
		u8 b12:1;
		u8 b13:1;
		u8 b14:1;
		u8 b15:1;
	}__packed ;
	u16 i;
} type_t;

typedef union {
	struct {
		u8 b0:3;
		u8 b1:4;
		u8 b2:6;
		u8 b3:3;
	}__packed;
	u16 i;
} type2_t;

int main(int argc, char *argv[]) {
	type_t t;
	type2_t t2;
	u16 i;
	bool passed;

	printf("sizeof(type_t): %ld\n", sizeof(type_t));
	for(i = 0; i < 16; i ++) {
		t.i = 0;
		switch(i) {
			case 0:
				t.b0 = 1;
				passed = (t.i == 0x0001);
				break;
			case 1:
				t.b1 = 1;
				passed = (t.i == 0x0002);
				break;
			case 2:
				t.b2 = 1;
				passed = (t.i == 0x0004);
				break;
			case 3:
				t.b3 = 1;
				passed = (t.i == 0x0008);
				break;
			case 4:
				t.b4 = 1;
				passed = (t.i == 0x0010);
				break;
			case 5:
				t.b5 = 1;
				passed = (t.i == 0x0020);
				break;
			case 6:
				t.b6 = 1;
				passed = (t.i == 0x0040);
				break;
			case 7:
				t.b7 = 1;
				passed = (t.i == 0x0080);
				break;
			case 8:
				t.b8 = 1;
				passed = (t.i == 0x0100);
				break;
			case 9:
				t.b9 = 1;
				passed = (t.i == 0x0200);
				break;
			case 10:
				t.b10 = 1;
				passed = (t.i == 0x0400);
				break;
			case 11:
				t.b11 = 1;
				passed = (t.i == 0x0800);
				break;
			case 12:
				t.b12 = 1;
				passed = (t.i == 0x1000);
				break;
			case 13:
				t.b13 = 1;
				passed = (t.i == 0x2000);
				break;
			case 14:
				t.b14 = 1;
				passed = (t.i == 0x4000);
				break;
			case 15:
				t.b15 = 1;
				passed = (t.i == 0x8000);
				break;
		}
		printf("%02u %04X %s\n", i, t.i, passed ? "\033[32mPASS\033[0m" : "\033[31mFAIL\033[0m");
	}
	printf("\n");

	printf("sizeof(type2_t): %ld\n", sizeof(type2_t));
	for(i = 0; i < 16; i ++) {
		t2.i = 0;
		switch(i) {
			case 0:
				t2.b0 = 1;
				passed = (t2.i == 0x0001);
				break;
			case 1:
				t2.b0 = 3;
				passed = (t2.i == 0x0003);
				break;
			case 2:
				t2.b0 = 7;
				passed = (t2.i == 0x0007);
				break;
			case 3:
				t2.b1 = 1;
				passed = (t2.i == 0x0008);
				break;
			case 4:
				t2.b1 = 3;
				passed = (t2.i == 0x0018);
				break;
			case 5:
				t2.b1 = 7;
				passed = (t2.i == 0x0038);
				break;
			case 6:
				t2.b1 = 15;
				passed = (t2.i == 0x0078);
				break;
			case 7:
				t2.b2 = 1;
				passed = (t2.i == 0x0080);
				break;
			case 8:
				t2.b2 = 3;
				passed = (t2.i == 0x0180);
				break;
			case 9:
				t2.b2 = 7;
				passed = (t2.i == 0x0380);
				break;
			case 10:
				t2.b2 = 15;
				passed = (t2.i == 0x0780);
				break;
			case 11:
				t2.b2 = 31;
				passed = (t2.i == 0x0F80);
				break;
			case 12:
				t2.b2 = 63;
				passed = (t2.i == 0x1F80);
				break;
			case 13:
				t2.b3 = 1;
				passed = (t2.i == 0x2000);
				break;
			case 14:
				t2.b3 = 3;
				passed = (t2.i == 0x6000);
				break;
			case 15:
				t2.b3 = 7;
				passed = (t2.i == 0xE000);
				break;
		}
		printf("%02u %04X %s\n", i, t2.i, passed ? "\033[32mPASS\033[0m" : "\033[31mFAIL\033[0m");
	}
	
	return 0;
}

