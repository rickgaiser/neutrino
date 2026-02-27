UDPFS_UDPTTY ?= 0
UDPFS_BD ?= 0
UDPFS_IOMAN ?= 0
UDPFS_FHI ?= 0

IOP_CFLAGS += -mno-check-zero-division -I../smap/include

IOP_OBJS_COMMON = main.o ministack_eth.o ministack_arp.o ministack_ip.o ministack_udp.o imports.o exports.o

ifeq ($(UDPFS_UDPTTY),1)
  IOP_CFLAGS += -DFEATURE_UDPTTY
  IOP_OBJS = $(IOP_OBJS_COMMON) udptty.o
  IOP_OBJS_DIR = obj.udptty/
  IOP_BIN = udpfs_udptty.irx
else ifeq ($(UDPFS_BD),1)
  IOP_CFLAGS += -DFEATURE_UDPTTY -DUDPRDMA_MAX_SOCKETS=1 -DFEATURE_UDPFS_BD
  IOP_OBJS = $(IOP_OBJS_COMMON) udptty.o udprdma.o udpfs_core.o udpfs_bd.o
  IOP_OBJS_DIR = obj.udpfs_bd/
  IOP_BIN = udpfs_bd.irx
else ifeq ($(UDPFS_IOMAN),1)
  IOP_CFLAGS += -DFEATURE_UDPTTY -DUDPRDMA_MAX_SOCKETS=1 -DFEATURE_UDPFS_IOMAN
  IOP_OBJS = $(IOP_OBJS_COMMON) udptty.o udprdma.o udpfs_core.o udpfs_ioman.o
  IOP_OBJS_DIR = obj.udpfs_ioman/
  IOP_BIN = udpfs_ioman.irx
else ifeq ($(UDPFS_FHI),1)
  IOP_CFLAGS += -DFEATURE_UDPTTY -DUDPRDMA_MAX_SOCKETS=1 -DFEATURE_UDPFS_FHI
  IOP_OBJS = $(IOP_OBJS_COMMON) udptty.o udprdma.o udpfs_core.o udpfs_fhi.o
  IOP_OBJS_DIR = obj.udpfs_fhi/
  IOP_BIN = udpfs_fhi.irx
endif

include ../Rules.make
