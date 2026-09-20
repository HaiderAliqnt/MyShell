CC       := gcc
TARGET   := mysh

SRC_DIR  := src
INC_DIR  := include
LIB_DIR  := libs/linenoise
TEST_DIR := tests
BUILD    := build

# -g -O0 keeps gdb sessions readable. Use `make OPT=-O2` for an optimised build.
OPT      ?= -O0
CFLAGS   := -std=gnu11 -Wall -Wextra -g $(OPT) -I$(INC_DIR) -I$(LIB_DIR) -MMD -MP

SRCS     := $(wildcard $(SRC_DIR)/*.c)
OBJS     := $(patsubst $(SRC_DIR)/%.c,$(BUILD)/%.o,$(SRCS)) $(BUILD)/linenoise.o
# everything except main.o - reused by the unit tests
CORE_OBJS:= $(filter-out $(BUILD)/main.o,$(OBJS))

UNIT_TESTS := $(BUILD)/test_parser $(BUILD)/test_proctable

.PHONY: all run test unit-test func-test gdb-test gdb valgrind clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(BUILD)/%.o: $(SRC_DIR)/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/linenoise.o: $(LIB_DIR)/linenoise.c | $(BUILD)
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
