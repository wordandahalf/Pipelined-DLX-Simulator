FILES = src/sim.c src/assemble.c
TEST_RESULTS = test/.[0-9]*
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
	@./$(OUTPUT) -D programs/$(@F) > test/.$(@F)
	@$(CMP) test/.$(@F) test/$(@F)

all: clean $(OUTPUT)

$(OUTPUT):  src/sim.c src/assemble.c include/globals.h
	@$(CC) $(FILES) $(CFLAGS) $(LIBS) -o $(OUTPUT)
