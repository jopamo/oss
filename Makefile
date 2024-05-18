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
PGMGMT_DEPS = $(addprefix $(SRC_DIR)/, timeutils.c)
PGMGMT_VERSIONS = $(SRC_DIR)/psmgmtA3.c
WORKER_VERSIONS = $(SRC_DIR)/workerA3.c
TEST_SRC = $(wildcard $(TEST_DIR)/*.c)
TEST_COMMON_SRC = $(COMMON_SRC) $(PGMGMT_DEPS)

# Object Files
COMMON_OBJ = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(COMMON_SRC))
PGMGMT_DEPS_OBJ = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(PGMGMT_DEPS))
PGMGMT_OBJ = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(PGMGMT_VERSIONS))
WORKER_OBJ = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(WORKER_VERSIONS))
TEST_OBJ = $(patsubst $(TEST_DIR)/%.c,$(TEST_OBJ_DIR)/%.o,$(TEST_SRC))
TEST_COMMON_OBJ = $(patsubst $(SRC_DIR)/%.c,$(TEST_OBJ_DIR)/%.o,$(TEST_COMMON_SRC))

# Executables
PGMGMT_EXECUTABLE = $(BIN_DIR)/psmgmt
WORKER_EXECUTABLE = $(BIN_DIR)/worker
TEST_EXECUTABLES = $(patsubst $(TEST_DIR)/%.c,$(TEST_BIN_DIR)/%,$(TEST_SRC))

# Targets
.PHONY: all clean directories test

all: directories $(PGMGMT_EXECUTABLE) $(WORKER_EXECUTABLE)

$(PGMGMT_EXECUTABLE): $(OBJ_DIR)/psmgmt.o $(PGMGMT_OBJ) $(COMMON_OBJ) $(PGMGMT_DEPS_OBJ)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^

$(WORKER_EXECUTABLE): $(OBJ_DIR)/worker.o $(WORKER_OBJ) $(COMMON_OBJ)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

directories:
	mkdir -p $(OBJ_DIR) $(BIN_DIR) $(TEST_OBJ_DIR) $(TEST_BIN_DIR)

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
