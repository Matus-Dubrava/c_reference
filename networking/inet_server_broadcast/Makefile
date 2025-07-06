CC = clang
CLFAGS = -Wall -Werror -Wextra -g
BUILD_DIR = build
SERV_SRC = src/server.c
CLIENT_SRC = src/client.c
SERV_OUT = $(BUILD_DIR)/server
CLIENT_OUT = $(BUILD_DIR)/client

server:
	mkdir -p $(BUILD_DIR)
	$(CC) $(SERV_SRC) $(CFLAGS) -o $(SERV_OUT)

server-run: server
	./$(SERV_OUT)

.PHONY: server