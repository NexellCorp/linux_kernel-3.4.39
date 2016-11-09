#CC=arm-generic-linux-gnueabi-gcc
#CC=arm-none-eabi-gcc
#CC=arm-linux-gnueabi-gcc
#CC=/usr/local/CodeSourcery/Sourcery_G++_Lite/bin/arm-none-eabi-gcc
CC=arm-linux-gcc

TARGET		= testmtv

INCLUDES	= -I. -I./.. -I./../mtv319 
SUBLIBS		= -lpthread -lm -lrt
CFLAGS		= -Wall -D_REENTRANT 
LDFLAGS		= 

SRCS    = test.c test_freq_tbl.c test_tdmb.c mtv319_ficdec.c
OBJS	= $(SRCS:.c=.o)

.SUFFIXES: .c .o

.c.o:
	$(CC) -c $(CFLAGS) $(INCLUDES) -o $@ $<

$(TARGET):	$(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(SUBLIBS) -static

all:	$(TARGET)

clean:
	rm -f $(TARGET) *.o *.a *~

distclean:
	rm -f $(TARGET) *.o *.a *.bak *~ .depend

dep:    depend

depend:
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend


#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif

