CC      = gcc
CFLAGS  = -Wall -Wextra -Wpedantic -std=c11 -O2 -g \
          -I include \
          -D_POSIX_C_SOURCE=200809L
LDFLAGS = -lpthread

SRC_DIR  = src
OBJ_DIR  = build
BIN_DIR  = bin

SRCS     = $(wildcard $(SRC_DIR)/*.c)
OBJS     = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))

SERVER   = $(BIN_DIR)/server
CLIENT   = $(BIN_DIR)/client

.PHONY: all clean dirs

all: dirs $(SERVER) $(CLIENT)

dirs:
	mkdir -p $(OBJ_DIR) $(BIN_DIR)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(SERVER): $(OBJ_DIR)/frame.o $(OBJ_DIR)/reliable.o $(OBJ_DIR)/transport.o \
           $(OBJ_DIR)/conn.o $(OBJ_DIR)/server.o
	$(CC) $^ -o $@ $(LDFLAGS)

$(CLIENT): $(OBJ_DIR)/frame.o $(OBJ_DIR)/reliable.o $(OBJ_DIR)/transport.o \
           $(OBJ_DIR)/conn.o $(OBJ_DIR)/client.o
	$(CC) $^ -o $@ $(LDFLAGS)

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)
