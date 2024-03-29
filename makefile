CC = gcc
CFLAGS = -Wall -g
#List of dependencies (.h)
DEPS = BPB.h DIRENTRY.h
#List of object files (.o)
OBJ = interpret.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)
	
make: $(OBJ)
	$(CC) -o project3 $^ $(CFLAGS)

clean:
	rm *.o
	rm project3
