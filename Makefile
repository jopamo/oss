# Compiler settings
CC = gcc
CFLAGS = -fsanitize=address -fsanitize=undefined -Wall -Wextra -pedantic -g3 -O0 -Werror -DDEBUG -fpic -fpie -fstack-protector-all
INCLUDES = -Iinclude

# Directories
SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin
TEST_DIR = test
TEST_OBJ_DIR = $(OBJ_DIR)/test

# Source Files
COMMON_SRC = $(addprefix $(SRC_DIR)/, arghandler.c shared.c resource.c user_process.c globals.c cleanup.c)
OSS_SRC = $(COMMON_SRC) $(SRC_DIR)/oss.c $(SRC_DIR)/process.c
WORKER_SRC = $(COMMON_SRC) $(SRC_DIR)/worker.c
TIMEKEEPER_SRC = $(COMMON_SRC) $(SRC_DIR)/timekeeper.c
TABLEPRINTER_SRC = $(COMMON_SRC) $(SRC_DIR)/tableprinter.c
TEST_SOURCES = $(wildcard $(TEST_DIR)/*.c)
TEST_EXECUTABLES = $(patsubst $(TEST_DIR)/%.c, $(BIN_DIR)/%, $(TEST_SOURCES))

# Object Files
COMMON_OBJ = $(COMMON_SRC:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
OSS_OBJ = $(OSS_SRC:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
WORKER_OBJ = $(WORKER_SRC:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
TIMEKEEPER_OBJ = $(TIMEKEEPER_SRC:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
TABLEPRINTER_OBJ = $(TABLEPRINTER_SRC:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

# Targets
.PHONY: all clean directories test

all: directories test_executables oss worker timekeeper tableprinter

test: $(TEST_EXECUTABLES)
	for test_exec in $(TEST_EXECUTABLES); do \
		./$$test_exec; \
	done

directories:
	mkdir -p $(OBJ_DIR) $(BIN_DIR) $(TEST_OBJ_DIR)

oss: $(OSS_OBJ)
	$(CC) $(CFLAGS) -o $(BIN_DIR)/$@ $^

worker: $(WORKER_OBJ)
	$(CC) $(CFLAGS) -o $(BIN_DIR)/$@ $^

timekeeper: $(TIMEKEEPER_OBJ)
	$(CC) $(CFLAGS) -o $(BIN_DIR)/$@ $^

tableprinter: $(TABLEPRINTER_OBJ)
	$(CC) $(CFLAGS) -o $(BIN_DIR)/$@ $^

test_executables: $(TEST_EXECUTABLES)

$(BIN_DIR)/%: $(TEST_DIR)/%.c $(COMMON_OBJ)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(TEST_OBJ_DIR)/%.o: $(TEST_DIR)/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR)/* $(BIN_DIR)/* $(TEST_OBJ_DIR)/*
	mkdir -p $(OBJ_DIR) $(BIN_DIR) $(TEST_OBJ_DIR)
