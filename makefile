MASTER        = master
MASTER_OBJ    = master.o $(SHARED_O)
MASTER_H      = $(SHARED_H)

BIN_ADDER     = bin_adder
BIN_ADDER_OBJ = bin_adder.o $(SHARED_O)
BIN_ADDER_H   = $(SHARED_H)

TEST_GEN      = randomTestGen
TEST_GEN_OBJ  = randomTestGen.o

SHARED_H  = sharedMemory.h perrorExit.h shmkey.h constants.h
SHARED_O  = sharedMemory.o perrorExit.o 

OUTPUT     = $(MASTER) $(BIN_ADDER)
OUTPUT_OBJ = $(MASTER_OBJ) $(BIN_ADDER_OBJ)
CC         = gcc
FLAGS      = -Wall -g -lpthread -lm
METHOD	   = #-DM2
SLEEP	   = #-DNOSLEEP

.SUFFIXES: .c .o

all: $(OUTPUT)
testgen: $(TEST_GEN)

$(MASTER): $(MASTER_OBJ) $(MASTER_H)
	$(CC) $(FLAGS) -o $@ $(MASTER_OBJ)

$(BIN_ADDER): $(BIN_ADDER_OBJ) $(BIN_ADDER_H)
	$(CC) $(FLAGS) -o $@ $(BIN_ADDER_OBJ)

$(TEST_GEN): $(TEST_GEN_OBJ)
	$(CC) $(FLAGS) -o $@ $(TEST_GEN_OBJ)

.c.o:
	$(CC) $(FLAGS) $(METHOD) $(SLEEP) -c $<

.PHONY: clean rmfile cleanall
clean:
	/bin/rm -f $(OUTPUT) $(OUTPUT_OBJ)
cleantestgen:
	/bin/rm -f $(TEST_GEN) $(TEST_GEN_OBJ)
rmfiles:
	/bin/rm -f adder_log semaphore_log test time_log
cleanall:
	/bin/rm -f adder_log semaphore_log time_log $(OUTPUT) $(TEST_GEN) *.o

