build:
	@echo "Compiling sources..."
	gcc -Wall -o server server.c
	gcc -Wall -o client client.c

clean:
	@echo "Cleaning binaries..."
	rm server
	rm client

delete_received:
	@echo "Deleting received files..."
	rm received_*