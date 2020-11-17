SRC_DIR := src
OBJ_DIR := obj
BIN_DIR := bin

EXE1 := $(BIN_DIR)/oss
EXE2 := $(BIN_DIR)/user
FIL1 := $(OBJ_DIR)/user.o
FIL2 := $(OBJ_DIR)/oss.o
SRC := $(wildcard $(SRC_DIR)/*.c)
OBJ := $(SRC:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

CC := gcc --std=c99 -D_XOPEN_SOURCE
CFLAGS := -Iinclude -MMD -MP -Wall -g

.PHONY: all clean cleanrun

all: $(EXE1) $(EXE2)

$(EXE1): $(filter-out $(FIL1), $(OBJ)) | $(BIN_DIR)
	$(CC) $^ -o $@

$(EXE2): $(filter-out $(FIL2), $(OBJ)) | $(BIN_DIR)
	$(CC) $^ -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN_DIR) $(OBJ_DIR):
	mkdir -p $@

clean:
	@$(RM) -rv $(BIN_DIR) $(OBJ_DIR)

cleanrun:
	@$(RM) -rv $(BIN_DIR)/*.out $(BIN_DIR)/*.log

-include $(OBJ:.o=.d)
