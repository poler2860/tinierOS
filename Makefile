ifndef DEBUG
# Default: compile for debug
DEBUG=1
endif

#PROFILE=1

CC = gcc

BASICFLAGS= -pthread -std=c11 -fno-builtin-printf 

DEBUGFLAGS=  -g3 
OPTFLAGS= -g3 -finline -march=native -O3 -DNDEBUG

ifeq ($(PROFILE),1)
PROFFLAGS= -g -pg 
PLFLAGS= -g -pg
else
PROFFLAGS= 
PLFLAGS=
endif

INCLUDE_PATH=-I.

CFLAGS= -Wall -D_GNU_SOURCE $(BASICFLAGS)

ifeq ($(DEBUG),1)
CFLAGS+=  $(DEBUGFLAGS) $(PROFFLAGS) $(INCLUDE_PATH)
else
CFLAGS+=  $(OPTFLAGS) $(PROFFLAGS) $(INCLUDE_PATH)
endif

LDFLAGS= $(PLFLAGS) $(BASICFLAGS)
LIBS=-lpthread -lrt -lm


C_PROG= mtask.c tinyos_shell.c terminal.c 

#
#  Add kernel source files here
#
C_SRC= bios.c $(wildcard kernel_*.c) tinyoslib.c symposium.c console.c
C_OBJ=$(C_SRC:.c=.o)

C_SOURCES= $(C_PROG) $(C_SRC)
C_OBJECTS=$(C_SOURCES:.c=.o)

FIFOS= con0 con1 con2 con3 kbd0 kbd1 kbd2 kbd3

.PHONY: all clean distclean doc shorthelp help depend

all: shorthelp mtask tinyos_shell terminal fifos 

#
# Normal apps
#

mtask: mtask.o $(C_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tinyos_shell: tinyos_shell.o $(C_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

terminal: terminal.o 
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

# fifos

fifos: $(FIFOS)

$(FIFOS):
	mkfifo $@

doc: tinyos3.cfg $(wildcard *.h)
	doxygen tinyos3.cfg

distclean: realclean
	-touch .depend
	-rm *~

realclean:
	-rm $(C_PROG:.c=) $(C_OBJECTS) .depend
	-rm $(FIFOS)

depend: $(C_SOURCES)
	$(CC) $(CFLAGS) -MM $(C_SOURCES) > .depend

clean: realclean depend

ifeq ($(wildcard .depend),)
$(warning No .depend file found. Running recursive make to create it')
$(info $(shell touch .depend && make depend))
include .depend
else
include .depend
endif

shorthelp:
	@echo Type \'make help\' to get information on running make

help: manhelp.man
	man -l manhelp.man

manhelp.man: manhelp.md
	pandoc manhelp.md -s -t man > manhelp.man
