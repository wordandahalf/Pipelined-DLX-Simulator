FILES = sim.c assemble.c
TEST_RESULTS = test/.1 test/.2 test/.3 test/.4 test/.5
OUTPUT = sim

CC = gcc
CFLAGS = -g -w
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

$(OUTPUT):  sim.c assemble.c globals.h
	@$(CC) $(FILES) $(CFLAGS) $(LIBS) -o $(OUTPUT)
