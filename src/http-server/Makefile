httpserver : httpserver.o List.o 
	gcc -Wall -Wextra -Wpedantic -Wshadow -o httpserver httpserver.o List.o -pthread
httpserver.o : httpserver.c
	gcc -Wall -Wextra -Wpedantic -Wshadow -c httpserver.c -pthread
List.o : List.c List.h
	gcc -Wall -Wextra -Wpedantic -Wshadow -c List.c
clean :
	rm -f httpserver httpserver.o List.o


