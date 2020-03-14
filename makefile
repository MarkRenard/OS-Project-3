MASTER       = master
MASTER_OBJ   = master.o $(SHARED_O)
MASTER_H     = $(SHARED_H)

BIN_ADDER     = bin_adder
BIN_ADDER_OBJ = bin_adder.o $(SHARED_O)
BIN_ADDER_H   = $(SHARED_H)

SHARED_H  = sharedMemory.h perrorExit.h shmkey.h
SHARED_O  = sharedMemory.o perrorExit.o 

OUTPUT    = $(MASTER) $(BIN_ADDER)
CC        = gcc
FLAGS     = -Wall -g -lpthread

.SUFFIXES: .c .o


all: $(OUTPUT)

$(MASTER): $(MASTER_OBJ) $(MASTER_H)
	$(CC) $(FLAGS) -o $@ $(MASTER_OBJ)

$(BIN_ADDER): $(BIN_ADDER_OBJ) $(BIN_ADDER_H)
	$(CC) $(FLAGS) -o $@ $(BIN_ADDER_OBJ)

.c.o:
	$(CC) $(FLAGS) -c $<

.PHONY: clean rmfile cleanall
clean:
	/bin/rm -f $(OUTPUT) *.o 
rmfile:
	/bin/rm -f adder_log 
cleanall:
	/bin/rm -f adder_log $(OUTPUT) *.o

