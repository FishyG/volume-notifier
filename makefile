CC=clang
CFLAGS=-Wall --include-directory=include 
LIBS=`pkg-config --cflags --libs libnotify libpipewire-0.3` -lm
TARGET=volume-notifier

# Files
OBJFILES=build/main.o build/volume_listener.o build/notification.o

all: release

release: CFLAGS += -O3
release: $(TARGET)

debug: CFLAGS += -Og -ggdb
debug: $(TARGET)

$(TARGET): $(OBJFILES)
	mkdir -p build
	@echo Compiling executable...
	@$(CC) $(CFLAGS) -o build/$(TARGET) $(OBJFILES) $(LIBS)

build/%.o: src/%.c
	@echo Compiling $@...
	@$(CC) $(CFLAGS) -c $< -o $@ $(LIBS)

clean:
	rm -rf build/*.o

.PHONY: main clean
