Robin Mathison
httpproxy readme

to compile:
make 
or
make httpproxy

to run:
./httpproxy <ports><flags>
	flag options:
	-N: Number of threads
	-R: Server Polling Frequency
	-s: cache size
	-m: cache item size capacity


all files must be in the same directory for proper functionality
files:
Makefile
httpproxy.c
Dictionary.c
Dictionary.h
List.c
List.h

The proxy can work with 1 or more instances of the server in /src/http-server


