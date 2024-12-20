include Makefile.inc

CC = gcc
CXX = g++
AR = ar
RL = ranlib

CFLAGS := $(CFLAGS) -O3 -I. -D_GNU_SOURCE -Wall -Wno-deprecated-declarations -Wno-pointer-sign -Wformat-truncation=0 -Wno-packed-bitfield-compat # -DHAVE_FTP_SSL
LFLAGS := $(LFLAGS) -lm -L. -Wl,-rpath,. -Wl,-rpath,$(PWD) # -lssl -lcrypto
CXXFLAGS :=
LXXFLAGS :=

CFLAGS += $(call cc-option,-Wno-unused-result,)

ifeq ($(MYSQL_CONFIG),)
MYSQL_CONFIG := $(shell which mysql_config 2>/dev/null)
ifeq ($(MYSQL_CONFIG),)
$(error Not found mysql_config command)
endif
endif
ifeq ($(shell $(MYSQL_CONFIG) --version 2>/dev/null),)
$(error $(MYSQL_CONFIG) not runnable)
endif
CFLAGS += $(shell $(MYSQL_CONFIG) --cflags)
LFLAGS += -Wl,-rpath,$(shell $(MYSQL_CONFIG) --variable=pkglibdir) $(shell $(MYSQL_CONFIG) --libs)

CFLAGS += $(shell pkg-config --cflags libmongoc-1.0 libbson-1.0)
LFLAGS += $(shell pkg-config --libs libmongoc-1.0 libbson-1.0)
CFLAGS += $(shell pkg-config --cflags glib-2.0 gdk-pixbuf-2.0)
LFLAGS += $(shell pkg-config --libs glib-2.0 gdk-pixbuf-2.0)
CFLAGS += $(shell pkg-config --cflags gtk+-2.0)
LFLAGS += $(shell pkg-config --libs gtk+-2.0)
CFLAGS += $(shell pkg-config --cflags libcjson)
LFLAGS += $(shell pkg-config --libs libcjson)

all:
	@echo -n

all: ss-test
ss-test: smart_str.o ss-test.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS) -pthread

all: struct-bit-field
struct-bit-field: struct-bit-field.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

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

all: BinaryTreeSort
BinaryTreeSort: BinaryTreeSort.o
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
	@$(CC) -o $@ $^ $(LFLAGS)

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

all: dirs-mysql
dirs-mysql: dirs-mysql.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

all: dirs-mongo
dirs-mongo: dirs-mongo.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

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

all: algo-cbrt
algo-cbrt: algo-cbrt.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

all: re
re: re.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

all: YangHuiTriangle2
YangHuiTriangle2: YangHuiTriangle2.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

all: xiaotou
xiaotou: xiaotou.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

all: msg
msg: msg.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

all: sem
sem: sem.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS) -lpthread

all: mmap-mutex
mmap-mutex: mmap-mutex.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS) -lpthread

all: mmap-file
mmap-file: mmap-file.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS) -lpthread

all: fork
fork: fork.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS) -lpthread

all: dup
dup: dup.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

all: exec
exec: exec.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

all: fork2
fork2: fork2.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

all: fork3
fork3: fork3.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

all: fork4
fork4: fork4.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

all: fork5
fork5: fork5.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

all: fifo
fifo: fifo.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

all: alarm
alarm: alarm.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

all: alarm2
alarm2: alarm2.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

all: setitimer
setitimer: setitimer.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

all: timer
timer: timer.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS) -lrt

all: kill-block
kill-block: kill-block.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

all: sigset
sigset: sigset.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

all: sigaction
sigaction: sigaction.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

all: sig-thread
sig-thread: sig-thread.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS) -lpthread

all: 8queen
8queen: 8queen.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

all: expect
expect: expect.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS) -lutil

all: sem2
sem2: sem2.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS) -pthread

all: glib
glib: glib.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

all: crc16
crc16: crc16.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

all: crc16s
crc16s: crc16s.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

all: mprotect
mprotect: mprotect.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

all: reboot
reboot: reboot.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

all: screen2bmp
screen2bmp: screen2bmp.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

all: screen2jpg
screen2jpg: screen2jpg.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS) -ljpeg

all: screenshot
screenshot: screenshot.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS) -ljpeg -lX11 -lXrandr

all: fbgif
fbgif: fbgif.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS) -lgif -lm

all: mice
mice: mice.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

all: keyboard
keyboard: keyboard.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

all: mkfont
mkfont: mkfont.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

all: pstore
pstore: pstore.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

all: lsblk
lsblk: lsblk.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

all: conn-stat
conn-stat: conn-stat.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

all: thread
thread: thread.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS) -pthread

all: asound
asound: asound.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS) -lasound

all: asoundcap
asoundcap: asoundcap.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS) -lasound

all: asoundcap2
asoundcap2: asoundcap2.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS) -lasound -pthread

all: asoundcap3
asoundcap3: asoundcap3.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS) -lasound -pthread

all: asoundcap-gtk
asoundcap-gtk: asoundcap-gtk.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS) -lasound -pthread

all: weditor
weditor: weditor.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS) -lasound -pthread

all: x11winlist
x11winlist: x11winlist.o
	@echo LD $@
	@$(CXX) -o $@ $^ $(LXXFLAGS) -lX11

all: sigurg-server
sigurg-server: sigurg-server.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

all: sigurg-client
sigurg-client: sigurg-client.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

all: sigpipe-server
sigpipe-server: sigpipe-server.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

all: sigpipe-client
sigpipe-client: sigpipe-client.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

all: sighup
sighup: sighup.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

all: continuity-blank-line
continuity-blank-line: continuity-blank-line.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

all: chkpwd
chkpwd: chkpwd.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS) -lcrypt

all: YangHuiTriangle3
YangHuiTriangle3: YangHuiTriangle3.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

all: cpufreq
cpufreq: cpufreq.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

all: v4l2-camera
v4l2-camera: v4l2-camera.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

%.o: %.c
	@echo CC $^
	@$(CC) $(CFLAGS) -c $^ -o $@

%.o: %.cpp
	@echo CXX $^
	@$(CXX) $(CXXFLAGS) -c $^ -o $@

%.O: %.c
	@echo CC $^
	@$(CC) $(CFLAGS) -fpic -fPIC -c $^ -o $@

test: url greatestCommonDivisor re mkfont
	./url 'https://github.com/talent518/calc?as=23&ew=23#fdsdasdf'
	./greatestCommonDivisor 319 377
	./re 1w a123@456.com.cn .as123-12@zdd.com a.as@qee.cn '.'
	./mkfont fonts/12x22.png font_12x22 > font_12x22.h
	./mkfont fonts/18x32.png font_18x32 > font_18x32.h

clean:
	@echo $@
	@rm -vf *.o *.O *.s *.S *.e *.E $(shell cat Makefile | awk '{if($$1=="all:") print $$2;}')

