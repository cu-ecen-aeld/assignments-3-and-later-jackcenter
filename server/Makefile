TARGET ?= aesdsocket

BUILD_DIR := ./build
TGT_DIR := ./
SRC_DIR := ./src
INC_DIR := ./include

SRCS := $(wildcard $(SRC_DIR)/*.c)	# Find all c files
OBJS := $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)	# make object files for all c files 

CC ?= $(CROSS_COMPILE)gcc
LDFLAGS ?=
CFLAGS ?= -g -Wall -Werror -O0 -I$(INC_DIR)

all: $(TGT_DIR)/$(TARGET)

# Target to build final executable
$(TGT_DIR)/$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

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
