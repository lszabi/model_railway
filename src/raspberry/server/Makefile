all: server

server: main.o sha1.o socket.o websocket.o uart.o control.o
	gcc -g -std=gnu99 -o server $^

%.o : %.c
	gcc -g -std=gnu99 -c -o $@ $<
	
clean:
	rm -f server server.exe *.o