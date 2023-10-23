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

test: clean $(OUTPUT)
	@./$(OUTPUT) prog1 > test/.1
	@$(CMP) test/.1 test/1
	@./$(OUTPUT) prog2 > test/.2
	@$(CMP) test/.2 test/2
	@./$(OUTPUT) prog3 > test/.3
	@$(CMP) test/.3 test/3
	@./$(OUTPUT) prog4 > test/.4
	@$(CMP) test/.4 test/4
	@./$(OUTPUT) prog5 > test/.5
	@$(CMP) test/.5 test/5
	@echo "Tests passed successfully."


all: $(OUTPUT)

$(OUTPUT):  sim.c assemble.c globals.h
	@$(CC) $(FILES) $(CFLAGS) $(LIBS) -o $(OUTPUT)
