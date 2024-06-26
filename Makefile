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
TEST_BIN_DIR = $(BIN_DIR)/test

# Source Files
COMMON_SRC = $(addprefix $(SRC_DIR)/, arghandler.c cleanup.c shared.c signals.c process.c init.c resource.c user_process.c globals.c queue.c)
WORKER_VERSIONS = $(wildcard $(SRC_DIR)/workerA*.c)
PGMGMT_VERSIONS = $(wildcard $(SRC_DIR)/psmgmtA*.c)
PGMGMT_DEPS = $(addprefix $(SRC_DIR)/, timeutils.c)
TEST_SRC = $(wildcard $(TEST_DIR)/*.c)
TEST_COMMON_SRC = $(COMMON_SRC) $(PGMGMT_DEPS)

# Object Files
COMMON_OBJ = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(COMMON_SRC))
WORKER_OBJ = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(WORKER_VERSIONS))
PGMGMT_OBJ = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(PGMGMT_VERSIONS))
PGMGMT_DEPS_OBJ = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(PGMGMT_DEPS))
TEST_OBJ = $(patsubst $(TEST_DIR)/%.c,$(TEST_OBJ_DIR)/%.o,$(TEST_SRC))
TEST_COMMON_OBJ = $(patsubst $(SRC_DIR)/%.c,$(TEST_OBJ_DIR)/%.o,$(TEST_COMMON_SRC))

# Executables
WORKER_EXECUTABLE = $(patsubst $(SRC_DIR)/%.c,$(BIN_DIR)/%,$(WORKER_VERSIONS))
PGMGMT_EXECUTABLES = $(patsubst $(SRC_DIR)/%.c,$(BIN_DIR)/%,$(PGMGMT_VERSIONS))
TEST_EXECUTABLES = $(patsubst $(TEST_DIR)/%.c,$(TEST_BIN_DIR)/%,$(TEST_SRC))

# Targets
.PHONY: all clean directories test worker

all: directories $(PGMGMT_EXECUTABLES) worker

worker: $(WORKER_EXECUTABLE)

$(WORKER_EXECUTABLE): $(WORKER_OBJ) $(COMMON_OBJ)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

directories:
	mkdir -p $(OBJ_DIR) $(BIN_DIR) $(TEST_OBJ_DIR) $(TEST_BIN_DIR)

$(BIN_DIR)/%: $(OBJ_DIR)/%.o $(COMMON_OBJ) $(PGMGMT_DEPS_OBJ)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^

$(TEST_OBJ_DIR)/%.o: $(TEST_DIR)/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(TEST_BIN_DIR)/%: $(TEST_OBJ_DIR)/%.o $(TEST_COMMON_OBJ)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^

test: directories $(TEST_EXECUTABLES)
	@echo "Running tests..."
	@./run_tests.sh $(TEST_EXECUTABLES)

clean:
	rm -rf $(OBJ_DIR)/* $(BIN_DIR)/* $(TEST_OBJ_DIR)/* $(TEST_BIN_DIR)/*
	mkdir -p $(OBJ_DIR) $(BIN_DIR) $(TEST_OBJ_DIR) $(TEST_BIN_DIR)
