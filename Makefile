CC = gcc
CFLAGS = -Wall -g
LDFLAGS = 
OPENSSL_LIBS = -lssl -lcrypto

SOURCES = server.c client.c
OBJECTS = $(SOURCES:.c=.o)
EXECUTABLE = server client

all: $(EXECUTABLE)

server: server.o
	$(CC) $(LDFLAGS) -o $@ $^ $(OPENSSL_LIBS)

client: client.o
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(EXECUTABLE)
