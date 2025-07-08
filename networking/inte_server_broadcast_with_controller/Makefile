server:
	clang server.c -Wall -Werror -Wextra -g -Iinclude -o server
server-run: server
	./server

client:
	clang client.c -Wall -Werror -Wextra -g -Iinclude -o client 
client-run:client 
	./client

controller:
	clang controller.c -Wall -Werror -Wextra -g -Iinclude -o controller 
controller-run:controller 
	./controller

.PHONY: server server-run client client-run controller controller-run