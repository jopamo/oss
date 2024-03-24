CC = gcc
CFLAGS = -Wall -Wextra -pedantic -g
INCLUDES = -Iinclude

SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin

OSS_SRC = arghandler.c shared.c oss.c process.c cleanup.c
WORKER_SRC = arghandler.c shared.c worker.c cleanup.c

OSS_OBJ = $(OSS_SRC:%.c=$(OBJ_DIR)/%.o)
WORKER_OBJ = $(WORKER_SRC:%.c=$(OBJ_DIR)/%.o)

all: directories oss worker

directories:
	mkdir -p $(OBJ_DIR) $(BIN_DIR)

oss: $(OSS_OBJ)
	$(CC) $(CFLAGS) -o $(BIN_DIR)/oss $^

worker: $(WORKER_OBJ)
	$(CC) $(CFLAGS) -o $(BIN_DIR)/worker $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR)/ $(BIN_DIR)/
