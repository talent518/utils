CC=gcc
CFLAGS=-I. -fvisibility=hidden
LDFLAGS=-L. -Wl,-rpath,. -rdynamic -export-dynamic

all: shared

shared: main.o libshared.so
	@echo $@
	@$(CC) $(LDFLAGS) main.o -lshared -o $@

libshared.so: shared.o libshared2.so
	@echo $@
	@$(CC) -o $@ -shared -fpic -fPIC $(LDFLAGS) shared.o -lshared2

libshared2.so: shared2.o
	@echo $@
	@$(CC) -o $@ -shared -fpic -fPIC $(LDFLAGS) shared2.o

%.o: %.c
	@echo $^
	@$(CC) $(CFLAGS) -c $^ -o $@

clean:
	@echo $@
	@rm -vf shared *.o *.so

