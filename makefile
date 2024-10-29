CC=clang
CFLAGS=-Wall
LIBS=`pkg-config --cflags --libs libnotify libpipewire-0.3` -lm

all: release

release: main clean

debug: CFLAGS += -Og -ggdb
debug: main clean

main:
	mkdir -p build
	@echo Compiling executable
	@$(CC) $(CFLAGS) -o build/main main.c $(LIBS)

clean:
	rm -rf build/*.o
