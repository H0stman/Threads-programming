
ifeq ($(OS), Windows_NT)
	DELETE := del
else
	DELETE := rm
endif

## Compile command variables
FLAGS :=-Wpedantic -pedantic-errors -Wextra
LIBS :=m

SRCS = $(wildcard *.c)
PROGS = $(patsubst %.c,%,$(SRCS))

all: $(PROGS)

%: %.c
	$(CC) $(CFLAGS)  -o $@ $<

clean:
	$(DELETE) *.o $(PROGS)
