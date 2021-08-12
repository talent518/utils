CC=g++
CFLAGS=-I. -fvisibility=hidden -fPIC -g
LDFLAGS=-L. -Wl,-rpath,. -rdynamic

all: shared

shared: main.o libshared.so
	@echo $@
	@$(CC) $(LDFLAGS) main.o -lshared -o $@

libshared.so: shared.o libshared2.so
	@echo $@
	@$(CC) -o $@ -shared -fpic $(LDFLAGS) shared.o -lshared2

libshared2.so: shared2.o
	@echo $@
	@$(CC) -o $@ -shared -fpic $(LDFLAGS) shared2.o

%.o: %.cpp
	@echo $^
	@$(CC) $(CFLAGS) -c $^ -o $@

clean:
	@echo $@
	@rm -vf shared *.o *.so

