#CC=gcc
CC=arm-linux-gnueabihf-gcc

CFLAGS:=-Wall -O3
CFLAGS += -DBINDER_IPC_32BIT

OBJS=src/binder_hal.o \
     src/binder_io.o \
     src/binder_ipc.o
SVC_SRC=service_manager/binder_srv_manager.c

LIB=libcbinder.so
SVC_MANAGER=svc_manager

INC_DIR=./include

all:$(LIB) $(SVC_MANAGER)

%.o : %.c
	$(CC) $(CFLAGS) -fpic -c $< -o $@ -I$(INC_DIR) -lpthread

$(SVC_MANAGER):
	$(CC) -o $(SVC_MANAGER) $(SVC_SRC) $(CFLAGS) -I$(INC_DIR) -L./ -lcbinder -lpthread

$(LIB) : $(OBJS)
	$(CC) -shared -o $@ $(OBJS)

clean:
	rm $(OBJS) $(LIB) $(SVC_MANAGER)
