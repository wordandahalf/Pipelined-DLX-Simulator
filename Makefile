FILES = sim.c assemble.c
TESTS = test/.1 test/.2 test/.3 test/.4 test/.5
OUTPUT = sim

CC = gcc
CFLAGS = -g -w
LIBS = -lm
RM = rm
CMP = cmp

clean:
	@$(RM) -f $(OUTPUT) $(TESTS)

test: clean test/1 test/2 test/3 test/4 test/5
	@echo "Tests passed successfully."

test/*: $(OUTPUT)
	@./$(OUTPUT) programs/$(@F) > test/.$(@F)
	@$(CMP) test/.$(@F) test/$(@F)

all: $(OUTPUT)

$(OUTPUT):  sim.c assemble.c globals.h
	@$(CC) $(FILES) $(CFLAGS) $(LIBS) -o $(OUTPUT)
