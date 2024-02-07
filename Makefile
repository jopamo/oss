# Define the compiler and compilation flags
CC=gcc
CFLAGS = -Wall -Wextra -pedantic -g

# Compile both oss and user executables as the default action
all: oss user

# Link the oss object file to create the oss executable
oss: oss.o
	$(CC) $(CFLAGS) -o oss oss.o

# Link the user object file to create the user executable
user: user.o
	$(CC) $(CFLAGS) -o user user.o

# Compile oss.c to an object file
oss.o: oss.c
	$(CC) $(CFLAGS) -c oss.c

# Compile user.c to an object file
user.o: user.c
	$(CC) $(CFLAGS) -c user.c

# Clean up by removing object files and executables
clean:
	rm -f *.o oss user
