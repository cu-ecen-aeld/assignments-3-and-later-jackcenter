TARGET_EXEC := aesdsocket

BUILD_DIR := ./build
SRC_DIR := ./

SRCS := $(wildcard $(SRC_DIR)/*.c)	# Find all c files
OBJS := $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)	# make object files for all c files 

cc = ($CROSS_COMPILE)gcc
CFLAGS = -g -Wall -Werror -O0

all: $(SRC_DIR)/$(TARGET_EXEC)

# Target to build final executable
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
	