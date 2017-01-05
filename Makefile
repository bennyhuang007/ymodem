#CROSS_COMPILE:=arm-poky-linux-gnueabi-
#ARCH=arm
#CC:=$(CROSS_COMPILE)gcc
#LD:=$(CROSS_COMPILE)ld
#
#all:send receive
#
#serial.o:serial.c serial.h
#
#send:send.o serial.o
#	$(CC) -o send -Wall send.o serial.o
#
#receive:receive.o serial.o
#	$(CC) -o receive -Wall receive.o serial.o
#
#.PHONY : clean
#clean : 
#	-rm send receive *.o

CROSS_COMPILE:=
ARCH=arm
CC:=$(CROSS_COMPILE)gcc
LD:=$(CROSS_COMPILE)ld

all:send

serial.o:serial.c serial.h

send:send.o serial.o
	$(CC) -o send -Wall send.o serial.o

receive:receive.o serial.o
	$(CC) -o receive -Wall receive.o serial.o

.PHONY : clean
clean : 
	-rm send receive *.o

