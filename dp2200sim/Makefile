OBJS=main.o dp2200_cpu_sim.o cassetteTape.o dp2200_io_sim.o dp2200Window.o CommandWindow.o RegisterWindow.o FloppyDrive.o

CPP=c++
CC=cc
SDL2_LIBS   := $(shell sdl2-config --libs)
SDL2_CFLAGS := $(shell sdl2-config --cflags)
FLAGS=-std=c++17 -Werror -Wall -Wno-unused-result -O3
CXXFLAGS := $(FLAGS) $(SDL2_CFLAGS)
LDFLAGS  := -lncurses -lform $(SDL2_LIBS)
5500FIRMWARE=5500firmware.inverted.bin
CREATEHEADER=createHeaderFromBin.c

.PHONY: all
all: dp2200sim

%.o:%.cpp
	$(CPP) -c $(CXXFLAGS) -o $@ $^

convertInstructionSetToHeader: convertInstructionSetToHeader.cpp
	$(CPP) $(CFLAGS) -o $@ $^

datapoint_instruction_set.h: datapoint_instruction_set.tsv convertInstructionSetToHeader
	./convertInstructionSetToHeader < datapoint_instruction_set.tsv > $@
	$(CPP) $(CFLAGS) -o verifyInstructionSetHeader verifyInstructionSetHeader.cpp
	./verifyInstructionSetHeader < datapoint_instruction_set.tsv 

5500firmware.h: $(CREATEHEADER) $(5500FIRMWARE)
	$(CC) -o createHeaderFromBin $(CREATEHEADER)
	./createHeaderFromBin < $(5500FIRMWARE) > $@

dp2200sim: datapoint_instruction_set.h 5500firmware.h $(OBJS)
	$(CPP) $(CXXFLAGS) $(OBJS) -o dp2200sim $(LDFLAGS)

.PHONY: clean

clean:
	@rm -f $(OBJS) createHeaderFromBin 5500firmware.h verifyInstructionSetHeader convertInstructionSetToHeader datapoint_instruction_set.h
 


