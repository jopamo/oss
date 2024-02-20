# Define the compiler and compilation flags
CC=gcc
CFLAGS = -Wall -Wextra -pedantic -g

# Compile both oss and worker executables as the default action
all: oss worker

# Link the oss object file to create the oss executable
oss: oss.o
	$(CC) $(CFLAGS) -o oss oss.o

# Link the worker object file to create the worker executable
worker: worker.o
	$(CC) $(CFLAGS) -o worker worker.o

# Compile oss.c to an object file
oss.o: oss.c
	$(CC) $(CFLAGS) -c oss.c

# Compile worker.c to an object file
worker.o: worker.c
	$(CC) $(CFLAGS) -c worker.c

# Clean up by removing object files and executables
clean:
	rm -f *.o oss worker
