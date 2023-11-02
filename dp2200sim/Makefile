OBJS=main.o dp2200_cpu_sim.o cassetteTape.o dp2200_io_sim.o

CC=c++

all: dp2200sim

dp2200sim: $(OBJS)
	$(CC) -lncurses -lform -o $@ $?

%.o:%.cpp
	$(CC) -c -std=c++17  -o $@ $^

.PHONY: clean

clean:
	@rm -f $(OBJS)
	@rm -f papertape
	@rm -f leader space punchedpapertape 

