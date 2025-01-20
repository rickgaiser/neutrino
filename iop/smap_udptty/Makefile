IOP_SRC_DIR ?= ../smap_udpbd/src/

IOP_CFLAGS += -mno-check-zero-division
IOP_CFLAGS += -DNO_BDM
IOP_OBJS = main.o smap.o xfer.o ministack.o udptty.o imports.o exports.o

include ../../Defs.make
include ../Rules.bin.make
include ../Rules.make
