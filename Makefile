CC      = gcc
CFLAGS  = -O2 -Wall -Wextra -std=c11
LDFLAGS =

BINS = malbolge malbolge-ipc

.PHONY: all test clean ipc

all: $(BINS)

malbolge: src/vm.c src/malbolge.c src/vm.h
	$(CC) $(CFLAGS) -o malbolge src/vm.c src/malbolge.c $(LDFLAGS)

malbolge-ipc: src/vm.c src/ipc.c src/vm.h
	$(CC) $(CFLAGS) -o malbolge-ipc src/vm.c src/ipc.c $(LDFLAGS)

ipc: malbolge-ipc

test: all
	@echo "Running hello_world test..."
	@printf '' | ./malbolge examples/hello.malbolge 100000 2>/dev/null | head -1

clean:
	rm -f $(BINS)
