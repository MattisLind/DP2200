FROM ubuntu:22.04
RUN apt update
RUN apt upgrade -y
RUN apt install -y gcc
RUN apt install -y g++
RUN apt-get install -y libncurses5-dev libncursesw5-dev
RUN apt-get install -y make
RUN mkdir dp2200
COPY . dp2200
RUN cd dp2200; make clean; make