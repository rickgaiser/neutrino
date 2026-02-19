SMAP_UDPTTY ?= 0
SMAP_UDPFS_BD ?= 0
SMAP_UDPFS_IOMAN ?= 0
SMAP_UDPFS_FHI ?= 0

IOP_CFLAGS += -mno-check-zero-division

IOP_OBJS_COMMON = main.o smap.o xfer.o ministack_eth.o ministack_arp.o ministack_ip.o ministack_udp.o imports.o exports.o

ifeq ($(SMAP_UDPTTY),1)
  IOP_CFLAGS += -DFEATURE_UDPTTY
  IOP_OBJS = $(IOP_OBJS_COMMON) udptty.o
  IOP_OBJS_DIR = obj.udptty/
  IOP_BIN = smap_udptty.irx
else ifeq ($(SMAP_UDPFS_BD),1)
  IOP_CFLAGS += -DFEATURE_UDPTTY -DUDPRDMA_MAX_SOCKETS=1 -DFEATURE_UDPFS_BD
  IOP_OBJS = $(IOP_OBJS_COMMON) udptty.o udprdma.o udpfs_core.o udpfs_bd.o
  IOP_OBJS_DIR = obj.udpfs_bd/
  IOP_BIN = smap_udpfs_bd.irx
else ifeq ($(SMAP_UDPFS_IOMAN),1)
  IOP_CFLAGS += -DFEATURE_UDPTTY -DUDPRDMA_MAX_SOCKETS=1 -DFEATURE_UDPFS_IOMAN
  IOP_OBJS = $(IOP_OBJS_COMMON) udptty.o udprdma.o udpfs_core.o udpfs_ioman.o
  IOP_OBJS_DIR = obj.udpfs_ioman/
  IOP_BIN = smap_udpfs_ioman.irx
else ifeq ($(SMAP_UDPFS_FHI),1)
  IOP_CFLAGS += -DFEATURE_UDPTTY -DUDPRDMA_MAX_SOCKETS=1 -DFEATURE_UDPFS_FHI
  IOP_OBJS = $(IOP_OBJS_COMMON) udptty.o udprdma.o udpfs_core.o udpfs_fhi.o
  IOP_OBJS_DIR = obj.udpfs_fhi/
  IOP_BIN = smap_udpfs_fhi.irx
endif

include ../Rules.make
