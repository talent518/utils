CC = gcc
AR = ar
RL = ranlib

CFLAGS = -O3 -I. -Wno-unused-result -Wno-format -D_GNU_SOURCE # -DHAVE_FTP_SSL
LFLAGS = -lm -L. -Wl,-rpath,. -Wl,-rpath,$(PWD) # -lssl -lcrypto

all: cpu-memory-info
cpu-memory-info: cpu-memory-info.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

all: nonRepetitiveSequence
nonRepetitiveSequence: nonRepetitiveSequence.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

all: crypt
crypt: base64.o md5.o crypt.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

all: url
url: url.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

all: 9x9
9x9: 9x9.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

all: 3Angle
3Angle: 3Angle.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

all: YangHuiTriangle
YangHuiTriangle: YangHuiTriangle.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

all: BubbleSort
BubbleSort: BubbleSort.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

all: 5AngleStar
5AngleStar: 5AngleStar.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

all: mac
mac: mac.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

all: cpuid
cpuid: cpuid.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

all: greatestCommonDivisor
greatestCommonDivisor: greatestCommonDivisor.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

all: libftp.a
libftp.a: ftp.o
	@echo AR $@
	@$(AR) -rcs $@ $^

all: libftp.so
libftp.so: ftp.O
	@echo LD $@
	@$(CC) -shared -o $@ $^ $(LFLAGS)

all: rftp
rftp: getcmdopt.o rftp.o libftp.a
	@echo LD $@
	@$(CC) -o $@ $(filter %.o, $^) -lftp $(LFLAGS)

all: PI
PI: PI.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

all: PI1000
PI1000: PI1000.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

all: time
time: time.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

all: hanoi
hanoi: hanoi.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

all: algo-mul
algo-mul: algo-mul.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

all: algo-div
algo-div: algo-div.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

all: narcissistic-number
narcissistic-number: narcissistic-number.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

all: prime-factor
prime-factor: prime-factor.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

all: dirs
dirs: dirs.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

all: dirs-sqlite3
dirs-sqlite3: dirs-sqlite3.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS) -lsqlite3

all: algo-dec2bin
algo-dec2bin: algo-dec2bin.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

all: algo-dec2oct
algo-dec2oct: algo-dec2oct.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

all: algo-dec2hex
algo-dec2hex: algo-dec2hex.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

all: algo-base-convert
algo-base-convert: algo-base-convert.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

all: algo-four-rules-of-arithmetic
algo-four-rules-of-arithmetic: algo-four-rules-of-arithmetic.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

all: algo-sqrt
algo-sqrt: algo-sqrt.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

%.o: %.c
	@echo CC $^
	@$(CC) $(CFLAGS) -o $(@:.o=.s) -S $^
	@$(CC) $(CFLAGS) -o $(@:.o=.e) -E $^
	@$(CC) $(CFLAGS) -c $^ -o $@

%.O: %.c
	@echo CC $^
	@$(CC) $(CFLAGS) -fpic -fPIC -o $(@:.O=.S) -S $^
	@$(CC) $(CFLAGS) -fpic -fPIC -o $(@:.O=.E) -E $^
	@$(CC) $(CFLAGS) -fpic -fPIC -c $^ -o $@

test: url greatestCommonDivisor
	./url 'https://github.com/talent518/calc?as=23&ew=23#fdsdasdf'
	./greatestCommonDivisor 319 377

clean:
	@echo $@
	@rm -vf *.o *.O *.s *.S *.e *.E $(shell cat Makefile | awk '{if($$1=="all:") print $$2;}')

