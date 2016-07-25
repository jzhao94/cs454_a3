all:
	g++ -std=c++0x -pthread -c rpc.cpp
	g++ -std=c++0x -o binder binder.cpp
	g++ -std=c++0x -c client1.c
	g++ -std=c++0x -pthread -c server_function_skels.c server_functions.c server.c
	g++ -c server_functions.c server_function_skels.c server.c
	ar rvs librpc.a rpc.o
	g++ -L. client1.o -lrpc -pthread -std=c++0x -o client
	g++ -L. server_functions.o server_function_skels.o server.o -lrpc -pthread -std=c++0x -o server
	
clean:
	rm *.o
	rm librpc.a binder

	
