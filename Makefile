FILES = src/sim.c src/assemble.c
TEST_RESULTS = test/.1 test/.2 test/.3 test/.4 test/.5 test/.6
OUTPUT = sim

CC = gcc
CFLAGS = -g -w -Iinclude/
LIBS = -lm
RM = rm
CMP = cmp

clean:
	@$(RM) -f $(OUTPUT) $(TEST_RESULTS)

test: clean test/*
	@echo "Tests passed successfully."

test/*: $(OUTPUT)
	@./$(OUTPUT) programs/$(@F) > test/.$(@F)
	@$(CMP) test/.$(@F) test/$(@F)

all: clean $(OUTPUT)

$(OUTPUT):  src/sim.c src/assemble.c include/globals.h
	@$(CC) $(FILES) $(CFLAGS) $(LIBS) -o $(OUTPUT)
