IOP_SRC_DIR ?= ../smap_udpbd/src/

IOP_CFLAGS += -mno-check-zero-division
IOP_CFLAGS += -DNO_BDM
IOP_CFLAGS += -I../smap_udpbd/include
IOP_OBJS = main.o smap.o xfer.o ministack_eth.o ministack_arp.o ministack_ip.o ministack_udp.o udptty.o imports.o exports.o

include ../Rules.make
