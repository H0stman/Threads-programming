
ifeq ($(OS), Windows_NT)
	DELETE := del
else
	DELETE := rm
endif

## Compile command variables
FLAGS :=-Wpedantic -pedantic-errors -Wextra -g
LIBS :=-lm -lpthread

SRCS = $(wildcard *.c)
PROGS = $(patsubst %.c,%,$(SRCS))

all: $(PROGS)

%: %.c
	$(CC) $(FLAGS) -o $@ $< $(LIBS)

clean:
	$(DELETE) *.o $(PROGS)
