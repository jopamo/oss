CLANG_PATH = $(shell command -v clang)
CC = $(if $(CLANG_PATH),clang,gcc)

CFLAGS = -Wall -Wextra -pedantic -g3 -O0 -Werror
INCLUDES = -Iinclude

ifeq ($(CC), clang)
    FUZZ_FLAGS = -fsanitize=address
    CFLAGS += $(FUZZ_FLAGS)
    CFLAGS += -DDEBUG
else
endif

SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin

OSS_SRC = arghandler.c shared.c oss.c cleanup.c process.c
WORKER_SRC = arghandler.c shared.c worker.c process.c
TIMEKEEPER_SRC = arghandler.c shared.c timekeeper.c process.c
TABLEPRINTER_SRC = arghandler.c shared.c tableprinter.c process.c

OSS_OBJ = $(OSS_SRC:%.c=$(OBJ_DIR)/%.o)
WORKER_OBJ = $(WORKER_SRC:%.c=$(OBJ_DIR)/%.o)
TIMEKEEPER_OBJ = $(TIMEKEEPER_SRC:%.c=$(OBJ_DIR)/%.o)
TABLEPRINTER_OBJ = $(TABLEPRINTER_SRC:%.c=$(OBJ_DIR)/%.o)

all: directories oss worker timekeeper tableprinter

directories:
	mkdir -p $(OBJ_DIR) $(BIN_DIR)

oss: $(OSS_OBJ)
	$(CC) $(CFLAGS) -o $(BIN_DIR)/oss $^

worker: $(WORKER_OBJ)
	$(CC) $(CFLAGS) -o $(BIN_DIR)/worker $^

timekeeper: $(TIMEKEEPER_OBJ)
	$(CC) $(CFLAGS) -o $(BIN_DIR)/timekeeper $^

tableprinter: $(TABLEPRINTER_OBJ)
	$(CC) $(CFLAGS) -o $(BIN_DIR)/tableprinter $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR)/ $(BIN_DIR)/
