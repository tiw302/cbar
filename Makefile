# cbar - System status monitor

# --- Configuration ---

# Compiler settings
CC ?= gcc
CFLAGS += -std=c99 -Wall -Wextra -O2 -D_POSIX_C_SOURCE=200809L

# Libraries
LIBS =

# Paths
PREFIX ?= /usr/local
BINDIR = $(PREFIX)/bin

# Source file location (แก้ตรงนี้ให้ชี้ไปที่ src/)
SRC = src/main.c

# --- Build Rules ---

all: cbar

cbar: $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o cbar $(LIBS)

# --- Installation ---

install: all
	@echo "Installing cbar..."
	mkdir -p $(BINDIR)
	cp -f cbar $(BINDIR)
	chmod 755 $(BINDIR)/cbar

uninstall:
	rm -f $(BINDIR)/cbar

clean:
	rm -f cbar

.PHONY: all install uninstall clean
