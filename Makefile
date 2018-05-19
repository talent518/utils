CC = gcc

CFLAGS = -O3 -I.
LFLAGS = -lm

all: cpu-memory-info nonRepetitiveSequence crypt url 9x9 3Angle YangHuiTriangle BubbleSort 5AngleStar

cpu-memory-info: cpu-memory-info.o
	$(CC) -o $@ $? $(LFLAGS)

nonRepetitiveSequence: nonRepetitiveSequence.o
	$(CC) -o $@ $? $(LFLAGS)

crypt: base64.o md5.o crypt.o
	$(CC) -o $@ $? $(LFLAGS)

url: url.o
	$(CC) -o $@ $? $(LFLAGS)

9x9: 9x9.o
	$(CC) -o $@ $? $(LFLAGS)

3Angle: 3Angle.o
	$(CC) -o $@ $? $(LFLAGS)

YangHuiTriangle: YangHuiTriangle.o
	$(CC) -o $@ $? $(LFLAGS)

BubbleSort: BubbleSort.o
	$(CC) -o $@ $? $(LFLAGS)

5AngleStar: 5AngleStar.o
	$(CC) -o $@ $? $(LFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -o $(@:.o=.s) -S $?
	$(CC) $(CFLAGS) -o $(@:.o=.e) -E $?
	$(CC) $(CFLAGS) -c $? -o $@

test: url
	./url 'https://github.com/talent518/calc?as=23&ew=23#fdsdasdf'

clean:
	@rm -f *.o *.s *.e cpu-memory-info nonRepetitiveSequence crypt url 9x9 3Angle YangHuiTriangle BubbleSort 5AngleStar

