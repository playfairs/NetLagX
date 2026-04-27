CC = gcc
CXX = g++
ASM = nasm
OBJCC = clang
CFLAGS = -Wall -Wextra -std=c99 -pthread -g -O3 -march=native
CXXFLAGS = -Wall -Wextra -std=c++11 -pthread -g -O3 -march=native
ASMFLAGS = -f elf64 -D__NASM_VERSION_MAJOR__=2 -D__NASM_VERSION_MINOR__=16
OBJCFLAGS = -Wall -Wextra -std=c99 -pthread -g -O3 -march=native -framework Foundation -framework SystemConfiguration -framework CoreFoundation -framework IOKit
LDFLAGS = -pthread -lcurl -ljson-c
GTK_CFLAGS = $(shell pkg-config --cflags gtk+-3.0 2>/dev/null || echo "")
GTK_LIBS = $(shell pkg-config --libs gtk+-3.0 2>/dev/null || echo "")
TAILSCALE_CFLAGS = $(shell pkg-config --cflags libcurl json-c 2>/dev/null || echo "")
TAILSCALE_LIBS = $(shell pkg-config --libs libcurl json-c 2>/dev/null || echo "")
OBJC_LIBS = -framework Foundation -framework SystemConfiguration -framework CoreFoundation -framework IOKit

SRCDIR = src
INCDIR = include
OBJDIR = obj
BINDIR = bin

C_SOURCES = $(wildcard $(SRCDIR)/**/*.c)
ASM_SOURCES = $(wildcard $(SRCDIR)/**/*.asm)
OBJC_SOURCES = $(wildcard $(SRCDIR)/**/*.m)

C_OBJECTS = $(C_SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
ASM_OBJECTS = $(ASM_SOURCES:$(SRCDIR)/%.asm=$(OBJDIR)/%.o)
OBJC_OBJECTS = $(OBJC_SOURCES:$(SRCDIR)/%.m=$(OBJDIR)/%.o)

ALL_OBJECTS = $(C_OBJECTS) $(ASM_OBJECTS) $(OBJC_OBJECTS)

TARGET_CLI = $(BINDIR)/netlagx-cli
TARGET_GUI = $(BINDIR)/netlagx-gui
TARGET = $(BINDIR)/netlagx

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    PLATFORM = darwin
    CFLAGS += -DPLATFORM_DARWIN
    LDFLAGS += $(OBJC_LIBS)
else
    PLATFORM = linux
    CFLAGS += -DPLATFORM_LINUX
endif

MKDIR_P = mkdir -p

.PHONY: all clean install cli gui check-gtk

all: check-gtk $(TARGET_CLI) $(TARGET_GUI)

check-gtk:
	@pkg-config --exists gtk+-3.0 || (echo "GTK+3 not found. GUI will not be built." && echo "Install with: brew install gtk+3" && exit 0)

$(TARGET_CLI): $(ALL_OBJECTS) | $(BINDIR)
	$(CC) $(ALL_OBJECTS) $(LDFLAGS) -o $@

$(TARGET_GUI): GTK_CFLAGS += $(GTK_CFLAGS)
$(TARGET_GUI): GTK_LIBS += $(GTK_LIBS)
$(TARGET_GUI): $(ALL_OBJECTS) | $(BINDIR)
	$(CC) $(ALL_OBJECTS) $(LDFLAGS) $(GTK_LIBS) -o $@

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	@$(MKDIR_P) $(dir $@)
	$(CC) $(CFLAGS) $(GTK_CFLAGS) $(TAILSCALE_CFLAGS) -I$(INCDIR) -c $< -o $@

$(OBJDIR)/%.o: $(SRCDIR)/%.asm | $(OBJDIR)
	@$(MKDIR_P) $(dir $@)
	$(ASM) $(ASMFLAGS) $< -o $@

$(OBJDIR)/%.o: $(SRCDIR)/%.m | $(OBJDIR)
	@$(MKDIR_P) $(dir $@)
	$(OBJCC) $(OBJCFLAGS) -I$(INCDIR) -c $< -o $@

$(OBJDIR):
	$(MKDIR_P) $(OBJDIR)

$(BINDIR):
	$(MKDIR_P) $(BINDIR)

cli: $(TARGET_CLI)

gui: $(TARGET_GUI)

clean:
	rm -rf $(OBJDIR) $(BINDIR)

install: all
	@echo "Installing NetLagX."
	sudo cp $(TARGET_CLI) /usr/local/bin/netlagx
	sudo cp $(TARGET_GUI) /usr/local/bin/netlagx-gui 2>/dev/null || echo "GUI not installed (GTK+3 not available)"
	@echo "Installation complete. Use 'netlagx' for CLI or 'netlagx-gui' for GUI."

uninstall:
	@echo "Uninstalling NetLagX."
	sudo rm -f /usr/local/bin/netlagx /usr/local/bin/netlagx-gui
	@echo "Uninstallation complete."

run-cli: $(TARGET_CLI)
	sudo $(TARGET_CLI)

run-gui: $(TARGET_GUI)
	sudo $(TARGET_GUI)

help:
	@echo "NetLagX Build System"
	@echo "Targets:"
	@echo "  all      - Build both CLI and GUI versions"
	@echo "  cli      - Build CLI version only"
	@echo "  gui      - Build GUI version only"
	@echo "  clean    - Remove build artifacts"
	@echo "  install  - Install to system (requires sudo)"
	@echo "  uninstall- Remove from system (requires sudo)"
	@echo "  run-cli  - Build and run CLI (requires sudo)"
	@echo "  run-gui  - Build and run GUI (requires sudo)"
	@echo "  help     - Show this help message"
	@echo ""
	@echo "Dependencies:"
	@echo "  - GCC compiler"
	@echo "  - GTK+3 (for GUI, optional)"
	@echo "  - Root privileges (for packet interception)"

debug: CFLAGS += -DDEBUG -g3
debug: all