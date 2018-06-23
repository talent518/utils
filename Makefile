CC = gcc

CFLAGS = -O3 -I.
LFLAGS = -lm

all: cpu-memory-info nonRepetitiveSequence crypt url 9x9 3Angle YangHuiTriangle BubbleSort 5AngleStar mac cpuid greatestCommonDivisor

cpu-memory-info: cpu-memory-info.o
	@echo LD $@
	@$(CC) -o $@ $? $(LFLAGS)

nonRepetitiveSequence: nonRepetitiveSequence.o
	@echo LD $@
	@$(CC) -o $@ $? $(LFLAGS)

crypt: base64.o md5.o crypt.o
	@echo LD $@
	@$(CC) -o $@ $? $(LFLAGS)

url: url.o
	@echo LD $@
	@$(CC) -o $@ $? $(LFLAGS)

9x9: 9x9.o
	@echo LD $@
	@$(CC) -o $@ $? $(LFLAGS)

3Angle: 3Angle.o
	@echo LD $@
	@$(CC) -o $@ $? $(LFLAGS)

YangHuiTriangle: YangHuiTriangle.o
	@echo LD $@
	@$(CC) -o $@ $? $(LFLAGS)

BubbleSort: BubbleSort.o
	@echo LD $@
	@$(CC) -o $@ $? $(LFLAGS)

5AngleStar: 5AngleStar.o
	@echo LD $@
	@$(CC) -o $@ $? $(LFLAGS)

mac: mac.o
	@echo LD $@
	@$(CC) -o $@ $? $(LFLAGS)

cpuid: cpuid.o
	@echo LD $@
	@$(CC) -o $@ $? $(LFLAGS)

greatestCommonDivisor: greatestCommonDivisor.o
	@echo LD $@
	@$(CC) -o $@ $? $(LFLAGS)

%.o: %.c
	@echo CC  $?
	@$(CC) $(CFLAGS) -o $(@:.o=.s) -S $?
	@$(CC) $(CFLAGS) -o $(@:.o=.e) -E $?
	@$(CC) $(CFLAGS) -c $? -o $@

test: url greatestCommonDivisor
	./url 'https://github.com/talent518/calc?as=23&ew=23#fdsdasdf'
	./greatestCommonDivisor 319 377

clean:
	@echo $@
	@rm -f *.o *.s *.e cpu-memory-info nonRepetitiveSequence crypt url 9x9 3Angle YangHuiTriangle BubbleSort 5AngleStar mac cpuid greatestCommonDivisor

