CC = gcc
CFLAGS = -Wall -Wextra -O2

TARGET = cbar
SRC = src/main.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGET)
