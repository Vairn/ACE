# Minimal Bartman GCC build wrapper for ACE diagnostics.

BARTMAN_BIN ?= $(HOME)/.vscode/extensions/bartmanabyss.amiga-debug-1.7.9/bin/darwin
TOOLCHAIN_BIN := $(BARTMAN_BIN)/opt/bin
TOOLCHAIN_PATH ?= $(BARTMAN_BIN)/opt
TOOLCHAIN_CMAKE ?= $(HOME)/Amiga/games/miami-quack-82/deps/AmigaCMakeCrossToolchains/m68k-bartman.cmake

BUILD_DIR := build
GAME_ELF := $(BUILD_DIR)/diagnostics.elf
GAME_EXE := $(BUILD_DIR)/diagnostics.exe

export PATH := $(TOOLCHAIN_BIN):$(BARTMAN_BIN):$(PATH)

.PHONY: all clean configure

all: $(GAME_EXE)

$(GAME_EXE): $(BUILD_DIR)/Makefile $(wildcard src/*.c src/*.h src/tests/*.c src/tests/*.h) CMakeLists.txt
	$(MAKE) -C $(BUILD_DIR)

$(BUILD_DIR)/Makefile: CMakeLists.txt
	mkdir -p $(BUILD_DIR)
	cmake -B $(BUILD_DIR) -S . \
		-DCMAKE_TOOLCHAIN_FILE=$(TOOLCHAIN_CMAKE) \
		-DTOOLCHAIN_PATH=$(TOOLCHAIN_PATH) \
		-DTOOLCHAIN_PREFIX=m68k-amiga-elf \
		-DM68K_CPU=68000 \
		-DCMAKE_BUILD_TYPE=Debug

configure: $(BUILD_DIR)/Makefile

clean:
	rm -rf $(BUILD_DIR)
