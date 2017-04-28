
ifeq ($(origin OS),undefined)
OS:=unknown
endif

ifeq ($(OS),Windows_NT)
EXE_EXT:=.exe
else
EXE_EXT:=
endif

RSCOPE:=rscope$(EXE_EXT)

all: $(RSCOPE)

.PHONY: all clean

CFLAGS:=-g -O2 -Wall -Wextra -Wformat-security -Wmissing-prototypes
LDFLAGS:=-Wall
LIBS:=-lgd -lm
CC:=gcc

rscope.o: rscope.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(RSCOPE): rscope.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

clean:
	rm -f *.o $(RSCOPE)

