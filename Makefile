# Define the compiler and compilation flags
CC = gcc
CFLAGS = -Wall -Wextra -pedantic -g -MMD -MP
INCLUDES = -Iinclude

# Directories
SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin

# Source files grouped by target
OSS_SOURCES = $(wildcard $(SRC_DIR)/oss.c $(SRC_DIR)/init.c $(SRC_DIR)/process.c $(SRC_DIR)/time.c $(SRC_DIR)/ipc.c $(SRC_DIR)/cleanup.c $(SRC_DIR)/util.c $(SRC_DIR)/shared.c)
WORKER_SOURCES = $(wildcard $(SRC_DIR)/worker.c $(SRC_DIR)/util.c $(SRC_DIR)/shared.c $(SRC_DIR)/ipc.c $(SRC_DIR)/init.c)

# Object files derived from source files, stored in obj directory
OSS_OBJECTS = $(OSS_SOURCES:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
WORKER_OBJECTS = $(WORKER_SOURCES:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

# Dependency files
DEP_FILES = $(OSS_OBJECTS:.o=.d) $(WORKER_OBJECTS:.o=.d)

# Default target
all: directories oss worker

# Ensure the obj and bin directories exist
directories:
	mkdir -p $(OBJ_DIR) $(BIN_DIR)

# Compile and link the oss executable
oss: $(OSS_OBJECTS)
	$(CC) $(CFLAGS) -o $(BIN_DIR)/$@ $^

# Compile and link the worker executable
worker: $(WORKER_OBJECTS)
	$(CC) $(CFLAGS) -o $(BIN_DIR)/$@ $^

# Generic rule for compiling source files to object files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Clean up build artifacts
clean:
	rm -rf $(OBJ_DIR)/* $(BIN_DIR)/* $(DEP_FILES)

# Include the .d files
-include $(DEP_FILES)
