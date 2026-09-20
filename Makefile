CC       := gcc
TARGET   := mysh
PREFIX   := CS311_A01_2024385

SRC_DIR  := src
INC_DIR  := include
TEST_DIR := tests
BUILD    := build

# linenoise.c may sit directly in lib/ or in a subfolder (and may carry the prefix)
LIB_SRC  := $(firstword $(wildcard lib/*linenoise.c lib/*/*linenoise.c))
LIB_DIR  := $(dir $(LIB_SRC))
ifeq ($(LIB_SRC),)
$(error linenoise.c not found under lib/ - check where you put it)
endif

# -g -O0 keeps gdb sessions readable. Use `make OPT=-O2` for an optimised build.
OPT      ?= -O0
CFLAGS   := -std=gnu11 -Wall -Wextra -g $(OPT) -I$(INC_DIR) -I$(LIB_DIR) -MMD -MP

SRCS     := $(wildcard $(SRC_DIR)/*.c)
OBJS     := $(patsubst $(SRC_DIR)/%.c,$(BUILD)/%.o,$(SRCS)) $(BUILD)/linenoise.o
# everything except main - reused by the unit tests
MAIN_OBJ := $(BUILD)/$(PREFIX)_main.o
CORE_OBJS:= $(filter-out $(MAIN_OBJ),$(OBJS))

# every tests/test_*.c becomes its own unit-test program
UNIT_TESTS := $(patsubst $(TEST_DIR)/%.c,$(BUILD)/%,$(wildcard $(TEST_DIR)/test_*.c))

.PHONY: all run test unit-test func-test gdb-test gdb valgrind clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(BUILD)/%.o: $(SRC_DIR)/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/linenoise.o: $(LIB_SRC) | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/test_%: $(TEST_DIR)/test_%.c $(CORE_OBJS) | $(BUILD)
	$(CC) $(CFLAGS) -I$(TEST_DIR) -o $@ $< $(CORE_OBJS)

$(BUILD):
	mkdir -p $(BUILD)

run: $(TARGET)
	./$(TARGET)

# ---- tests ----------------------------------------------------------------
test: unit-test func-test

unit-test: $(UNIT_TESTS)
	@for t in $(UNIT_TESTS); do ./$$t 2>/dev/null || exit 1; done

func-test: $(TARGET)
	@bash $(TEST_DIR)/run_tests.sh

# ---- debugging ------------------------------------------------------------
gdb-test: $(TARGET) $(UNIT_TESTS)
	@bash $(TEST_DIR)/gdb/run_gdb_tests.sh

gdb: $(TARGET)
	gdb -q ./$(TARGET)

valgrind: $(TARGET)
	@bash $(TEST_DIR)/run_valgrind.sh

clean:
	rm -rf $(BUILD) $(TARGET)

-include $(OBJS:.o=.d)