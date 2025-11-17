CXX ?= g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra

all: linuxDatapointCassetteTool linuxDatapointCassetteTool_fifo linuxDatapointCassetteTool_64bitacc linuxDatapointCassetteTool_64bitacc_critsections

linuxDatapointCassetteTool: linux_main.cpp arduino_shim.hpp ../tap_fm_tx_sketch.hpp
	$(CXX) $(CXXFLAGS) linux_main.cpp -DSTANDARD -o linuxDatapointCassetteTool

linuxDatapointCassetteTool_fifo: linux_main.cpp arduino_shim.hpp ../tap_fm_tx_sketch_fifo.hpp
	$(CXX) $(CXXFLAGS) linux_main.cpp -DFIFO -o linuxDatapointCassetteTool_fifo 

linuxDatapointCassetteTool_64bitacc: linux_main.cpp arduino_shim.hpp ../tap_fm_sketch_64bitacc.hpp
	$(CXX) $(CXXFLAGS) linux_main.cpp -DLONGACC -o linuxDatapointCassetteTool_64bitacc	

linuxDatapointCassetteTool_64bitacc_critsections: linux_main.cpp arduino_shim.hpp ../tap_fm_sketch_64bitacc_critsections.hpp
	$(CXX) $(CXXFLAGS) linux_main.cpp -DLONGACC_CRITSECTION -o linuxDatapointCassetteTool_64bitacc_critsections	

clean:
	rm -f linuxDatapointCassetteTool linuxDatapointCassetteTool_fifo linuxDatapointCassetteTool_64bitacc linuxDatapointCassetteTool_64bitacc_critsections