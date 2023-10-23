FILES = sim.c assemble.c
OUTPUT = sim

CC = gcc
CFLAGS = -g -w
LIBS = -lm
RM = rm

clean:
	$(RM) $(OUTPUT)

all: $(OUTPUT)

$(OUTPUT):  sim.c assemble.c globals.h
	$(CC) $(FILES) $(CFLAGS) $(LIBS) -o $(OUTPUT)
