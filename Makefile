CC = gcc
AR = ar
RL = ranlib

CFLAGS = -O3 -I. -Wno-unused-result -Wno-format -D_GNU_SOURCE # -DHAVE_FTP_SSL
LFLAGS = -lm -L. -Wl,-rpath,. -Wl,-rpath,$(PWD) # -lssl -lcrypto

all: cpu-memory-info nonRepetitiveSequence crypt url 9x9 3Angle YangHuiTriangle BubbleSort 5AngleStar mac cpuid greatestCommonDivisor libftp.a libftp.so rftp PI PI1000 time hanoi

cpu-memory-info: cpu-memory-info.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

nonRepetitiveSequence: nonRepetitiveSequence.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

crypt: base64.o md5.o crypt.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

url: url.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

9x9: 9x9.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

3Angle: 3Angle.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

YangHuiTriangle: YangHuiTriangle.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

BubbleSort: BubbleSort.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

5AngleStar: 5AngleStar.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

mac: mac.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

cpuid: cpuid.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

greatestCommonDivisor: greatestCommonDivisor.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

libftp.a: ftp.o
	@echo AR $@
	@$(AR) -rcs $@ $^

libftp.so: ftp.O
	@echo LD $@
	@$(CC) -shared -o $@ $^ $(LFLAGS)

rftp: getcmdopt.o rftp.o
	@echo LD $@
	@$(CC) -o $@ $^ -lftp $(LFLAGS)

PI: PI.o
	@echo LD $@
	@$(CC) -o $@ $^ -O3 $(LFLAGS)

PI1000: PI1000.o
	@echo LD $@
	@$(CC) -o $@ $^ -O3 $(LFLAGS)

time: time.o
	@echo LD $@
	@$(CC) -o $@ $^ -O3 $(LFLAGS)

hanoi: hanoi.o
	@echo LD $@
	@$(CC) -o $@ $^ -O3 $(LFLAGS)

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
	@rm -f *.o *.O *.s *.S *.e *.E *.a *.so cpu-memory-info nonRepetitiveSequence crypt url 9x9 3Angle YangHuiTriangle BubbleSort 5AngleStar mac cpuid greatestCommonDivisor rftp PI PI1000
