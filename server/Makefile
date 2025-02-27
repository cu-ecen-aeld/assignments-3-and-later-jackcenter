TARGET_EXEC := aesdsocket

BUILD_DIR := ./build
TGT_DIR := ./
SRC_DIR := ./src
INC_DIR := ./include

SRCS := $(wildcard $(SRC_DIR)/*.c)	# Find all c files
OBJS := $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)	# make object files for all c files 

cc = ($CROSS_COMPILE)gcc
CFLAGS = -g -Wall -Werror -O0 -I$(INC_DIR)

all: $(TGT_DIR)/$(TARGET_EXEC)

# Target to build final executable
$(TGT_DIR)/$(TARGET_EXEC): $(OBJS)
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
