CC ?= gcc
CFLAGS ?= -Wall -O2 -g
LDFLAGS ?=

VPATH=src

objects := builtin.o main.o parser.o cmd.o

.PHONY: clean

minishell: $(objects)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

main.o: main.c parser.h cmd.h
	$(CC) $(CFLAGS) -c $<

builtin.o: builtin.c builtin.h cmd.h
	$(CC) $(CFLAGS) -c $<

parser.o: parser.c parser.h
	$(CC) $(CFLAGS) -c $<

cmd.o: cmd.c cmd.h parser.h
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(objects)
	rm -f ./minishell
