CC = gcc
ID = ace
SRC = src
LIB_DIR = lib
TESTS_DIR = tests

BIN = $(ID)
LIB = $(LIB_DIR)/lib$(ID).a
TESTS = $(TESTS_DIR)/tests
BENCHMARK =  $(TESTS_DIR)/benchmark

CC_OPTIONS = -Wall -O1 -std=c99
LD_OPTIONS = -Llib -l$(ID)

all: $(BIN) $(LIB) 

tests: $(TESTS) $(BENCHMARK)


$(BIN): $(LIB) repl.c
	$(CC) $(CC_OPTIONS) -o $@ $^ $(LD_OPTIONS)
	
$(LIB): $(SRC)/$(ID).c
	$(CC) -c $(CC_OPTIONS) -o $(ID).o $^
	ar rc $(LIB) $(ID).o
	ranlib $(LIB)
	make clean
	
$(TESTS): $(LIB) $(TESTS_DIR)/tests.c
	$(CC) -o $(TESTS) $(TESTS_DIR)/tests.c $(LD_OPTIONS)
	$(TESTS)
	diff $(TESTS_DIR)/output $(TESTS_DIR)/expected_output
	
$(BENCHMARK): $(LIB)  $(TESTS_DIR)/benchmark.c
	$(CC) -o $(BENCHMARK) $(TESTS_DIR)/benchmark.c $(LD_OPTIONS)
	
clean:
	rm -f $(ID).o

	