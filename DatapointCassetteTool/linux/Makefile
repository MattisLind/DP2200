CXX ?= g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra

all: linuxDatapointCassetteTool

linuxDatapointCassetteTool: linux_main.cpp arduino_shim.hpp ../tap_fm_tx_sketch.hpp
	$(CXX) $(CXXFLAGS) linux_main.cpp -o linuxDatapointCassetteTool

clean:
	rm -f linuxDatapointCassetteTool