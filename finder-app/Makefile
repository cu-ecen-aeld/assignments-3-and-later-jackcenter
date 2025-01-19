
TARGET_EXEC := writer

BUILD_DIR := ./build
SRC_DIR := ./

SRCS := $(wildcard $(SRC_DIR)/*.c)	# Find all c files
OBJS := $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)	# make object files for all c files

CC = gcc
CFLAGS = -g

CROSS_COMPILE ?=

ifneq ($(CROSS_COMPILE),)
    CC := $(CROSS_COMPILE)gcc
endif

# Target to build final executable on native system
$(SRC_DIR)/$(TARGET_EXEC): $(OBJS)
	$(CC) $(OBJS) -o $@

# Target to build the object files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	@if [ -d "${BUILD_DIR}" ]; \
	then \
		rm -r ${BUILD_DIR}; \
	fi