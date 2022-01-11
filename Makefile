STANDALONE_TOOLCHAIN=~/ndk/bin/
target_host=$(STANDALONE_TOOLCHAIN)armv7a-linux-androideabi16
AR=$(target_host)-ar
AS=$(target_host)-clang
CC=$(target_host)-clang
CXX=$(target_host)-clang++
LD=$(target_host)-ld
STRIP=$target_host-strip

# Tell configure what flags Android requires.
CFLAGS=-pie -fPIC -O3
LDFLAGS=-pie
SOURCES=updown.c
OBJECTS=$(SOURCES:.cpp=.o)
EXECUTABLE=updown

all: $(SOURCES) $(EXECUTABLE)
$(EXECUTABLE): $(OBJECTS) 
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJECTS) -o $@
.c.o:
	$(CXX) $(CFLAGS) $< -o $@
.cpp.o:
	$(CXX) $(CFLAGS) $< -o $@

clean: 
	rm *.o updown consume eventgen monitor;

all: updown consume eventgen monitor
