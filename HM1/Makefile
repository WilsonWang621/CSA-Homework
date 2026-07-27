CC = gcc
CFLAGS = -Wall -Wextra -fPIC
TARGET = libhello.so

all: $(TARGET)

$(TARGET): hello.c hello.h
	$(CC) $(CFLAGS) -shared hello.c -o $(TARGET)

run: $(TARGET)
	python3 demo.py

clean:
	rm -f $(TARGET)

.PHONY: all run clean