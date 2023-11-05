OBJS=main.o dp2200_cpu_sim.o cassetteTape.o dp2200_io_sim.o

CC=c++
CFLAGS=-std=c++17 -Wall
LDFLAGS=-lncurses -lform
.PHONY: all
all: dp2200sim

%.o:%.cpp
	$(CC) -c  $(CFLAGS) -o $@ $^

dp2200sim: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o dp2200sim  $(LDFLAGS)

.PHONY: clean

clean:
	@rm -f $(OBJS)



