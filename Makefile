# ==============================================================================
# cbar - Ultra-lightweight status bar for i3wm / Sway
# ==============================================================================

# --- Configuration ---
CC      ?= gcc
PREFIX  ?= /usr/local
BINDIR   = $(PREFIX)/bin

# --- Compiler Flags ---
# -O2: Optimize for performance
# -Wall -Wextra: Show all warnings
CFLAGS  += -std=c99 -Wall -Wextra -O2 -D_POSIX_C_SOURCE=200809L
LDFLAGS += 

# --- Files & Directories ---
SRC_DIR  = src
SRC      = $(SRC_DIR)/main.c
OBJ      = $(SRC:.c=.o)
TARGET   = cbar

# --- Build Rules ---
.PHONY: all clean install uninstall

all: $(TARGET)

%.o: %.c $(SRC_DIR)/config.h
	@echo "  CC      $@"
	@$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJ)
	@echo "  LINK    $@"
	@$(CC) $(OBJ) $(LDFLAGS) -o $@
	@echo "  STRIP   $@"
	@strip $@

# --- Installation ---
install: all
	@echo "  INSTALL $(TARGET) -> $(DESTDIR)$(BINDIR)/$(TARGET)"
	@install -D -m 755 $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)

uninstall:
	@echo "  REMOVE  $(DESTDIR)$(BINDIR)/$(TARGET)"
	@rm -f $(DESTDIR)$(BINDIR)/$(TARGET)

clean:
	@echo "  CLEAN   build files"
	@rm -f $(TARGET) $(OBJ)
