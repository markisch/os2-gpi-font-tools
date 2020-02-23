CC      = gcc
INCDIR  = ../include
CFLAGS  = -Wall -g -I$(INCDIR)
#CFLAGS  = -Zomf -Zexe -g -I$(INCDIR)
OBJS    = os2font.o gpifont.o
OBJS2   = gpifont.o os2-to-bdf.o

all: os2font os2-to-bdf

os2-to-bdf : $(OBJS2)
		gcc $(CFLAGS) $(OBJS2) -o $@

os2font :	$(OBJS)
		gcc $(CFLAGS) $(OBJS) -o $@

clean :
		rm -f $(OBJS)
		rm -f os2font os2font.exe
