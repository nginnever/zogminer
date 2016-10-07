ifeq ($(shell uname -s),Darwin)
	CC ?= clang
	LDLIBS += -lcurl -framework OpenCL
else
	CC ?= gcc
	LDLIBS += -lOpenCL -lcurl
endif

CFLAGS += -c -std=c11 -Wall -pedantic -O2

TARGET = gpu-list cpu-list

SOURCES_GPU = gpu-list-gen.c
SOURCES_CPU = cpu-list-gen.c

OBJECTS_GPU = $(patsubst %.c,%.o,$(SOURCES_GPU))
OBJECTS_CPU = $(patsubst %.c,%.o,$(SOURCES_CPU))

all: $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -o $@ $<

gpu-list: $(OBJECTS_GPU)
	$(CC) -o $@ $^ $(LDLIBS)

cpu-list: $(OBJECTS_CPU)
	$(CC) -o $@ $^ $(LDLIBS)

clean:
	rm -f $(TARGET) $(OBJECTS_GPU) $(OBJECTS_CPU)

.PHONY: all clean
