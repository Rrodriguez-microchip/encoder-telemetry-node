# Host unit tests for the pure encoder logic (gcc + Unity, no hardware).
# Not part of the MPLAB X build. Run from SAMD21_Project/tests/:
#     make        build and run
#     make clean  remove build artifacts
#
# -I../src so includes match the firmware ("drivers/encoder_core.h").

CC      := gcc
CFLAGS  := -std=c11 -Wall -Wextra -Werror -g -I../src -Iunity
LDFLAGS :=

BUILD   := build
BIN     := $(BUILD)/test_encoder_core

SRCS := test_encoder_core.c \
        ../src/drivers/encoder_core.c \
        unity/unity.c

OBJS := $(SRCS:%.c=$(BUILD)/%.o)

.PHONY: all test clean
all: test

test: $(BIN)
	./$(BIN)

$(BIN): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD)
