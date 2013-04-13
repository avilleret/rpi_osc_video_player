OBJS=triangle.o video.o
BIN=osc_video_player.bin
LDFLAGS+=-lilclient -llo `pkg-config --libs opencv`
CFLAGS+=`pkg-config --cflags liblo` `pkg-config --cflags opencv`

include ../Makefile.include


