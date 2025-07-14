server:
	clang server.c queue.c -Wall -Werror -Wextra -g -Iinclude -pthread -gdwarf-4 -o build/server

server-run: server
	./build/server

server-vg: server
	valgrind --leak-check=full ./build/server

test-queue:
	clang tests/test_queue.c queue.c -Wall -Werror -Wextra -g -Iinclude -o build/test_queue
	./build/test_queue

client:
	go run client.go

.PHONY: server server-run test-queue