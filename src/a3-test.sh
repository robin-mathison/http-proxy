#!/bin/sh


BUF_SIZE=1000
TENMB=100000000

PORT=8080

let LARGE=$TENMB+10
let DOUBLE=$BUF_SIZE*2


#Test 12: Check if cache works with larger files (> 10MiB)
#Description: Start one httpserver instance. Start the proxy server
#with -m <size> where size greater than 20MB. Create a binary file with
#size greater than 10 MB and smaller than 20MB, Make 2 GET requests
#using curl to this file and save the contents to the output file using
#-o option.  Verify the contents of the file with diff. Also, verify
#the order of logging requests it should be GET, GET, HEAD.

echo "Creating random small binary file..."
head -c $LARGE </dev/urandom > t1
echo "Assuming 1 live server with initially empty log file called log1, saving outputs to t2 & t3"
curl http://localhost:$PORT/t1 -o t2 & curl http://localhost:$PORT/t1 -o t3
echo "running dif on t2 & t3..."
diff t2 t3 
head log1
echo "visually verify that the log entries are correct"

#Test 15: Check if cache updates files after changes.
#Description: Start one httpserver instance. Create a binary file and
#update its metadata with touch -m --date=yesterday <filename>. Start
#the proxy server with -R 10.  Make 2 GET requests using curl to this
#file and save the contents to the output file using the -o
#option. Save the contents of the original file. Update the contents of
#the original file using /dev/urandom. Make 2 GET requests to this
#file.  Verify the contents of the file with diff. Also, verify the
#order of logging requests it should be GET, GET, HEAD, HEAD, GET,
#HEAD.

#Test 18: Test cache dropping content when new version is too large.
#Description: Start one httpserver instance. Start the proxy server
#with -m 1000 -s 2 -R 20. Create 3 binary files with size 1000 bytes
#with /dev/urandom (a, b, c).  Update metadata of file b with touch -m
#--date=yesterday <filename>.  Make GET requests using curl to these
#files in the following order a, b.  Then save the contents of file b
#and update the contents of file b (make it bigger than the size of the
#cache entry). Then perform GET requests on the files in the following
#order b, c, c, b, a and save the contents to the output file using -o
#option.  Verify the contents of the files with diff. Also, verify the
#order of logging requests it should be GET, GET, GET, HEAD, GET, GET,
#HEAD, GET, HEAD.

#Test 22: Test if httpproxy updates cache (new version much larger, but inside limit).
#Description: Start one httpserver instance. Start the proxy server
#with -m 100000 -s 2 -R 20. Create a binary file with the size of 1000
#bytes with /dev/urandom.  Update metadata of the file with touch -m
#--date=yesterday <filename>.  Make 2 GET requests using curl to the
#file then save the contents of the file using the -o option. Save the
#original file then update the contents of the file (make it equal to
#the size of the cache entry).  Make 2 GET requests using curl to the
#file then save the contents of the file using -o option of the curl
#command.  Verify the contents of the files with diff. Also, verify the
#order of logging requests it should be GET, GET, HEAD, HEAD, GET,
#HEAD.

#Test 23: Test cache with a differently sized files (small, large, empty).
#Description: Start one httpserver instance. Start the proxy server
#with -m 100000 and -R 50. Create 3 binary files (a, b, c). File a with
#the size of 1000 bytes, file b with the size of 100000 bytes, and file
#c as an empty file.  Make GET requests to these files using curl in
#the following order a, b, c, a, b, c and save the contents of the file
#using the -o option of the curl command.  Verify the contents of the
#files with diff. Also, verify the order of logging requests it should
#be GET, GET, GET, GET, HEAD, HEAD, HEAD.

#Test 25: Test caching of many files under LRU.
#Description: Start one httpserver instance. Start the proxy server
#with -s 4 -R 50 -m 50000. Create 5 binary files (a, b, c, d, e). File
#a, c, e with the size of 1000 bytes, file b with the size of 10000
#bytes and file b with the size of 50000 bytes.  Make GET requests to
#these files using curl in the following order: a, b, c, d, b, e, c, a,
#d, e, a, d, b, c, a, d, e, a, e, c, b, e and save the contents of the
#file using the -o option of the curl command.  Verify the contents of
#the files with diff. Also, verify the order of logging requests it
#should be GET, GET, GET, GET, GET, HEAD, GET, HEAD, GET, GET, HEAD,
#HEAD, HEAD, GET, GET, HEAD, HEAD, GET, HEAD, HEAD, HEAD, GET, HEAD.

#Test 26: Test default values.
#Description: Start one httpserver instance (server1). Start the proxy
#server. Create five binary files (a, b, c, d, e) using
#/dev/urandom. File a should be large than the default value for -m and
#the others should have that exact value.  Perform five GET requests on
#the files in the following order a, a, b, c, d to proxy server and
#save the contents of the file using the -o option of the curl
#command. Then sleep for 0.5 sec. Then perform three GET requests on
#the files in the following order b, e, b to proxy server and save the
#contents of the file using the -o option of the curl command. Verify
#the contents of the files using diff. Also, verify the order of log
#entries on server1 logfile, it should be GET, GET, GET, GET, GET, GET,
#GET, HEAD, GET, GET.

#Test 27: Test if loadbalancer can forward traffic to a server that becomes the least loaded (external requests)
#Description: Start two httpserver instances (server1 and
#server2). Create two binary files (a, b) using /dev/urandom with
#different sizes.  Perform 3 GET requests on file a, to the
#server1. Start the proxy server with -R 3. Perform 4 GET requests on
#file a, on the server2. Perform 3 GET requests on file b, to the proxy
#server, and save the contents of the file using the -o option of the
#curl command. Verify the contents of the files using diff. Also,
#verify the number of log entries for server1 and server2. For server1
#it should be 6 and for server2 it should be 9.

#Test 28: Test if loadbalancer can handle concurrent requests including 400 and 404 requests.
#Description: Start two httpserver instances (server1 and
#server2). Create a binary file using /dev/urandom. Start the proxy
#server with -R 3. Perform the following 6 GET requests simultaneously
#using &. Three GET requests on the file and save the output using -o
#option of the curl command, one GET request with invalid file name,
#one GET request with invalid header (you can modify the header with
#--header option of the curl command), and one GET request for the file
#which does not exist. Record the http_code for later three GET
#commands using --write-out "%{http_code}" option of the curl
#command. Wait for all the requests to finish. Verify the contents of
#the file for the first 3 GET requests using diff. Also, verify the
#http_code for later 3 GET requests using grep. It should be 400 and
#404.''

#Test 29: Test if loadbalancer can forward a GET request to the least loaded server (2 servers have the same load)
#Description: Start three httpserver instances (server1, server2, and
#server3). Create a binary file using /dev/urandom. Perform three HEAD
#requests for the file to server1. Perform three HEAD requests for the
#file to server2. Start the proxy server. Perform 2 GET requests for
#the file to the proxy server and save the output using the -o option
#of the curl command. Verify the contents of the file. Also, verify the
#number of requests logged on each of the servers. It should be
#server1: 4, server2:4, server3: 3, Total: 11

#Test 30: Test if loadblancer can forward traffic to the least loaded server (many servers have the same load)
#Description: Start four httpserver instances (server1, server2,
#server3 and server4). Create a binary file using /dev/urandom. Start
#the proxy server. Perform 4 GET requests for the file to the proxy
#server and save the output using the -o option of the curl
#command. Verify the contents of the file. Also, verify the number of
#requests logged on each of the servers. It should be server1: 2,
#server2:2, server3: 2, server4: 2,Total: 8.

#Test 31: Test if loadbalancer can handle concurrent connections from multiple clients
#Description: Start three httpserver instances (server1, server2, and
#server3). Start the proxy server. Create three binary file using
#/dev/urandom to the server with different sizes(small, medium, large).
#Perform 3 GET requests for each file (total 9 requests) simultaneously
#using &. Save the contents of the file using the -o option of the curl
#command.  Verify the contents of the file using diff.

#Test 32: Test if loadbalancer can overcome a single server going down
#Description: Start two httpserver instances (server1 and
#server2). Create a binary file on the server using /dev/urandom.
#Perform three HEAD requests for the file to server2. Then start the
#proxy server. Kill the server1. Perform GET request for the file to
#the proxy server and save the contents of the file using the -o option
#of the curl command. Verify the contents of the file using diff.

#Test 33: Test if loadbalancer can detect multiple servers going down
#Description: Start five httpserver instances (server1, server2,
#server3, server4, and server5). Create a binary file on the server
#using /dev/urandom.  Perform three HEAD requests for the file to
#server5. Then start the proxy server. Kill server1, server2, server3,
#server4. Perform GET request for the file to the proxy server and save
#the contents of the file using the -o option of the curl
#command. Verify the contents of the file using diff.

#Test 34: Test if loadbalancer can detect a server coming back up
#Description: Start one httpserver instance (server2). Create a binary
#file on the server using /dev/urandom.  Perform three HEAD request for
#the file on server2. Then start the proxy server with two
#httpservers(one is already instantiated(server2) other server is
#down(server1)) and -R 3. Perform two GET requests. for the file to the
#proxy server and save the contents of the file using the -o option of
#the curl command. Start another httpserver instance(server1).  Perform
#two GET requests for the file to the proxy server and save the
#contents of the file using the -o option of the curl command.  Verify
#the contents of the file using diff. Also, verify the number of
#requests logged on each of the servers. It should be server1: 2,
#server2: 5, Total: 7.

#Test 35: Test if loadbalancer can ignore bad healthcheck responses (status code != 200)
#Description: Start two httpserver instances (server1 and
#server2). Server1 without logging and Server2 with logging enabled.
#Create a binary file on the server using /dev/urandom. Start the proxy
#server with -R 2. Then perform three GET requests for the file to the
#proxy server and save the contents of the file using the -o option of
#the curl command. Verify the contents of the file using diff.

