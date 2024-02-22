# Define the compiler and compilation flags
CC=gcc
CFLAGS=-Wall -Wextra -pedantic -g

# Compile both oss and worker executables as the default action
all: oss worker

# Link the oss object files to create the oss executable
oss: oss.o shared.o
	$(CC) $(CFLAGS) -o oss oss.o shared.o

# Link the worker object files to create the worker executable
worker: worker.o shared.o
	$(CC) $(CFLAGS) -o worker worker.o shared.o

# Compile oss.c to an object file
oss.o: oss.c shared.h
	$(CC) $(CFLAGS) -c oss.c

# Compile worker.c to an object file
worker.o: worker.c shared.h
	$(CC) $(CFLAGS) -c worker.c

# Compile shared.c to an object file
shared.o: shared.c shared.h
	$(CC) $(CFLAGS) -c shared.c

# Clean up by removing object files and executables
clean:
	rm -f *.o oss worker
