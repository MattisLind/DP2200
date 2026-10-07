UI_SOURCES=main.cpp dp2200_cpu_sim.cpp cassetteTape.cpp dp2200_io_sim.cpp dp2200Window.cpp CommandWindow.cpp RegisterWindow.cpp FloppyDrive.cpp
USE_SDL ?= 0
UI_BUILD_DIR=.ui-$(USE_SDL)
OBJS=$(UI_SOURCES:%.cpp=$(UI_BUILD_DIR)/%.o)

CPP=c++
CC=cc
PKG_CONFIG ?= pkg-config
SDL2_CONFIG ?=
ARCH_FLAGS ?=
ifeq ($(filter $(USE_SDL),0 1),)
$(error USE_SDL must be 0 or 1)
endif

# Installed SDL packages only. SDL2_CONFIG explicitly overrides pkg-config.
# Do not require SDL for the terminal UI or headless targets.
ifneq ($(filter 1,$(USE_SDL))$(filter test-sdl .console-test/check_console-sdl,$(MAKECMDGOALS)),)
ifneq ($(strip $(SDL2_CONFIG)),)
SDL2_AVAILABLE := $(shell "$(SDL2_CONFIG)" --version >/dev/null 2>&1 && echo yes)
else ifeq ($(shell $(PKG_CONFIG) --exists sdl2 2>/dev/null && echo yes),yes)
SDL2_AVAILABLE := yes
SDL2_CFLAGS := $(shell $(PKG_CONFIG) --cflags sdl2)
SDL2_LIBS := $(shell $(PKG_CONFIG) --libs sdl2)
SDL2_LIBDIR := $(shell $(PKG_CONFIG) --variable=libdir sdl2)
else
SDL2_CONFIG := sdl2-config
SDL2_AVAILABLE := $(shell "$(SDL2_CONFIG)" --version >/dev/null 2>&1 && echo yes)
endif
ifneq ($(SDL2_AVAILABLE),yes)
$(error SDL2 development package not found. macOS: brew install pkgconf sdl2-compat; Debian/Ubuntu: sudo apt install pkg-config libsdl2-dev. Or set SDL2_CONFIG=/path/to/sdl2-config)
endif
ifneq ($(strip $(SDL2_CONFIG)),)
SDL2_CFLAGS := $(shell "$(SDL2_CONFIG)" --cflags)
SDL2_LIBS := $(shell "$(SDL2_CONFIG)" --libs)
SDL2_LIBDIR := $(shell "$(SDL2_CONFIG)" --prefix)/lib
endif
ifeq ($(shell uname -s),Darwin)
SDL2_LIBS += -Wl,-rpath,"$(SDL2_LIBDIR)"
endif
endif
FLAGS=-std=c++17 -Werror -Wall -Wno-unused-result -O3 $(ARCH_FLAGS)
CXXFLAGS = $(FLAGS) -DDP2200_WITH_SDL=$(USE_SDL) $(if $(filter 1,$(USE_SDL)),$(SDL2_CFLAGS))
LDFLAGS  = -lncurses -lform $(if $(filter 1,$(USE_SDL)),$(SDL2_LIBS))
5500FIRMWARE=5500firmware.inverted.bin
CREATEHEADER=createHeaderFromBin.c

.PHONY: all
all: dp2200sim

$(OBJS): datapoint_instruction_set.h 5500firmware.h

$(UI_BUILD_DIR)/%.o:%.cpp $(wildcard *.h)
	mkdir -p $(UI_BUILD_DIR)
	$(CPP) -c $(CXXFLAGS) -o $@ $<

convertInstructionSetToHeader: convertInstructionSetToHeader.cpp
	$(CPP) $(CFLAGS) -o $@ $^

datapoint_instruction_set.h: datapoint_instruction_set.tsv convertInstructionSetToHeader
	./convertInstructionSetToHeader < datapoint_instruction_set.tsv > $@
	$(CPP) $(CFLAGS) -o verifyInstructionSetHeader verifyInstructionSetHeader.cpp
	./verifyInstructionSetHeader < datapoint_instruction_set.tsv 

5500firmware.h: $(CREATEHEADER) $(5500FIRMWARE)
	$(CC) -o createHeaderFromBin $(CREATEHEADER)
	./createHeaderFromBin < $(5500FIRMWARE) > $@

# Switching USE_SDL must relink even when that mode's objects already exist.
.PHONY: dp2200sim
dp2200sim: datapoint_instruction_set.h 5500firmware.h $(OBJS)
	$(CPP) $(CXXFLAGS) $(OBJS) -o dp2200sim $(LDFLAGS)

.PHONY: clean

clean:
	@rm -f .ui-0/*.o .ui-1/*.o $(UI_SOURCES:%.cpp=%.o) createHeaderFromBin 5500firmware.h verifyInstructionSetHeader convertInstructionSetToHeader datapoint_instruction_set.h $(HEADLESS_OBJECTS) dp2200sim-headless .console-test/check_console .console-test/check_console-sdl
 



# Separate objects avoid reusing old UI builds; no curses, forms or SDL needed.
HEADLESS_SOURCES=headless.cpp dp2200_cpu_sim.cpp dp2200_io_sim.cpp cassetteTape.cpp FloppyDrive.cpp
HEADLESS_OBJECTS=$(HEADLESS_SOURCES:%.cpp=.headless/%.o)

.headless/%.o: %.cpp datapoint_instruction_set.h 5500firmware.h $(wildcard *.h)
	mkdir -p .headless
	$(CPP) $(FLAGS) -c $< -o $@

dp2200sim-headless: $(HEADLESS_OBJECTS)
	$(CPP) $(FLAGS) $^ -o $@

.PHONY: test-headless
test-headless: dp2200sim-headless
	python3 -m unittest discover -s tests -p 'test_*.py' -v

.PHONY: test-console-diagnostics
test-console-diagnostics: dp2200sim-headless
	python3 -m unittest discover -s tests -p 'test_console_diagnostics.py' -v

.PHONY: test-rim
test-rim: dp2200sim-headless
	python3 -m unittest discover -s tests -p 'test_rim.py' -v

.PHONY: test-disks
test-disks: dp2200sim-headless
	python3 tests/disk_diagnostics.py

.PHONY: test-cassettes test-cassette-timing
test-cassettes: dp2200sim-headless
	python3 -m unittest discover -s tests -p 'test_cassette*.py' -v

test-cassette-timing: dp2200sim-headless
	python3 tests/cassette_diagnostics.py --require-timing

.PHONY: install-dos-d
install-dos-d: dp2200sim-headless
	python3 tests/install_dos_d.py

.PHONY: test-ui
test-ui: dp2200sim .console-test/check_console
	python3 tests/check_command_editor.py
	python3 tests/check_console.py

.console-test/check_console: tests/check_console.cpp dp2200Window.cpp $(wildcard *.h)
	mkdir -p .console-test
	$(CPP) $(FLAGS) -DDP2200_WITH_SDL=0 -I. tests/check_console.cpp dp2200Window.cpp -o $@ -lncurses

.PHONY: test-sdl .console-test/check_console-sdl
test-sdl: .console-test/check_console-sdl
	python3 tests/check_console.py --sdl

.console-test/check_console-sdl: tests/check_console.cpp dp2200Window.cpp $(wildcard *.h)
	mkdir -p .console-test
	$(CPP) $(FLAGS) -DDP2200_WITH_SDL=1 $(SDL2_CFLAGS) -I. tests/check_console.cpp dp2200Window.cpp -o $@ -lncurses $(SDL2_LIBS)

.PHONY: test-fonts
test-fonts: dp2200sim-headless
	python3 tests/inspect_fonts.py
