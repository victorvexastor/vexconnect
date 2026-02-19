CC = gcc
CFLAGS = -O2 -Wall -Wextra -std=c11
LDFLAGS =
SRC = src/main.c src/mesh.c src/packet.c src/seen.c src/crypto.c src/transport_unix.c src/util.c src/tweetnacl.c
TARGET = vexconnect

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
