httpproxy : httpproxy.o Dictionary.o List.o 
	gcc -Wall -Wextra -Wpedantic -Wshadow -o httpproxy httpproxy.o Dictionary.o List.o -pthread
httpproxy.o : httpproxy.c
	gcc -Wall -Wextra -Wpedantic -Wshadow -c httpproxy.c -pthread
Dictionary.o : Dictionary.c Dictionary.h
	gcc -Wall -Wextra -Wpedantic -Wshadow -c Dictionary.c
List.o : List.c List.h
	gcc -Wall -Wextra -Wpedantic -Wshadow -c List.c
clean :
	rm -f httpproxy httpproxy.o List.o Dictionary.o

