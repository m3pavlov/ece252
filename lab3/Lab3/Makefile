
# Makefile, ECE252  
# Yiqing Huang, 2019/06/12

CC = gcc       # compiler
CFLAGS = -Wall -g  -std=gnu99# compilation flg 
LD = gcc       # linker
LDFLAGS = -g   # debugging symbols in build
LDLIBS = -lcurl -pthread   # link with libz

# For students 
SRCS = paster2.c
OBJS1 = paster2.o $(LIB_UTIL)
TARGETS = paster2

all: ${TARGETS}

paster2: $(OBJS1) cURL.o crc.o zutil.o
	$(LD) -o $@ $^ $(LDLIBS) $(LDFLAGS) 

%.o: %.c 
	$(CC) $(CFLAGS) -c $< 

%.d: %.c
	gcc -MM -MF $@ $<

-include $(SRCS:.c=.d)

.PHONY: clean
clean:
	rm -f *.d *.o *~ $(TARGETS) *.png
