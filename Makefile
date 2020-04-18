all: game_client game_server 

LIBS = -lpthread
game_client: game_client.cpp 
	g++ -std=c++11 -g -o game_client game_client.cpp $(LIBS)

game_server: game_server.cpp
	g++ -std=c++11 -g -o game_server game_server.cpp $(LIBS)

clean: 
	rm -f *.o game_server
	rm -f *.o game_client
