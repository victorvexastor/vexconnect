CC = gcc
COSMOCC = $(HOME)/bin/cosmo/bin/cosmocc
CFLAGS = -O2 -Wall -Wextra -std=c11
SRC = src/main.c src/mesh.c src/packet.c src/seen.c src/crypto.c src/transport_unix.c src/util.c src/tweetnacl.c
TARGET = vexconnect

all: $(TARGET)

# Linux native binary (58KB)
$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^

# Actually Portable Executable — runs on Windows, Mac, Linux, FreeBSD (858KB)
portable: $(SRC)
	$(COSMOCC) $(CFLAGS) -o vexconnect.com $^

clean:
	rm -f $(TARGET) vexconnect.com

.PHONY: all portable clean
