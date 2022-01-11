#define _XOPEN_SOURCE 700
#define _GNU_SOURCE
#include <err.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <getopt.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "List.h"
#include "Dictionary.h"

#define BUF_SIZE 1000
#define BIG_BUF_SIZE 2500
#define HEAD_SIZE 100
#define DEFAULT_THREADS 5
#define DEFAULT_FILE 1024
#define DEFAULT_SIZE 3

pthread_mutex_t connfd_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t handle_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t get_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t cache_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t no_data;

uint16_t* ports;
int num_ports;
int poll_freq;
int num_requests;
int cache_size;
int file_capacity;
int replace;
int start;
char** replace_order;

List work_list;
List ordered_ports;
Dictionary response_cache;

/**
   Creates a socket for connecting to a server running on the same
   computer, listening on the specified port number.  Returns the
   socket file descriptor on succes.  On failure, returns -1 and sets
   errno appropriately.
 */
int create_client_socket(uint16_t port) {
  int clientfd = socket(AF_INET, SOCK_STREAM, 0);
  if (clientfd < 0) {
    return -1;
  }
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof addr);
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(port);
  if (connect(clientfd, (struct sockaddr*) &addr, sizeof addr)) {
    return -1;
  }
  return clientfd;
}


/**
   Converts a string to an 16 bits unsigned integer.
   Returns 0 if the string is malformed or out of the range.
 */
uint16_t strtouint16(char number[]) {
  char *last;
  long num = strtol(number, &last, 10);
  if (num <= 0 || num > UINT16_MAX || *last != '\0') {
    return 0;
  }
  return num;
}

/**
   Creates a socket for listening for connections.
   Closes the program and prints an error message on error.
 */
int create_listen_socket(uint16_t port2) {
  struct sockaddr_in addr;
  int listenfd = socket(AF_INET, SOCK_STREAM, 0);
  if (listenfd < 0) {
    err(EXIT_FAILURE, "socket error");
  }

  memset(&addr, 0, sizeof addr);
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htons(INADDR_ANY);
  addr.sin_port = htons(port2);
  if (bind(listenfd, (struct sockaddr*)&addr, sizeof addr) < 0) {
    err(EXIT_FAILURE, "bind error");
  }

  if (listen(listenfd, 500) < 0) {
    err(EXIT_FAILURE, "listen error");
  }

  return listenfd;
}

/*
Checks that the given resource name is valid
i.e. 19 characters, all ASCII, alphanumeric + '.' & '_'
returns 1 if valid and -1 if invalid
*/
int check_valid_resource(char* name){
  int len = strlen(name);

  if(len > 19){
    return (-1);
  }
  
  int asc;

  for(int i = 0; i < len; i++){
    asc = (int)name[i];
    if(asc < 46 || asc == 47 || (asc > 57 && asc < 65) || (asc > 90 && asc < 95) || 
      asc == 96 || asc > 122){
      return (-1);
    }
  }

  return 1;
}


/*
parses http request header
returns -1 if error/ not all required elements are found
returns 0 if all information is found
*/
int get_data(char* buffer, char* req_type, char* file_name, char* host_name){
  char** save = &buffer;
  char* token = strtok_r(buffer, " ", save);
  int val = 0;
  char temp[BUF_SIZE];

  while(token != NULL){
    if(val == 0){
      if(sscanf(token, "%s", req_type) == 1){
        val++;
      } 
    } 

    if(val == 1){
      if(sscanf(token, "/%s HTTP/1.1", file_name) == 1){
        if(token[strlen(token)-1] == '1'){
          val++;
        } else{
          return -1;
        }
      }
    }  

    if(val == 2){ 
      if(sscanf(token, "Host: %[^\n]", temp) == 1){
        char find_colon = '0';
        int count_steps = 0;
        int host_length = strlen(temp);

        while((find_colon != ':') && (count_steps < host_length)){
          if(find_colon == ' '){
            return -1;
          }

          count_steps++;
          find_colon = temp[host_length - count_steps];
        }

        if(count_steps == host_length){
          return -1;
        } else {
          strncpy(host_name, temp, (host_length - count_steps));
          return 0;
        }
      }
    }
    
    token = strtok_r(NULL, "\r\n", save);
  }

  return -1;
}

/*
* Probes each server to get healthcheck status
*/
void probe_servers(){

  /*
   * Account for cases where auxillirary servers
   * that do not support logging, mark them as bad
   */

  if(length(ordered_ports) > 0){
    clear(ordered_ports);
  }
  
  int connection;
  int errors, comp_errors;
  int entries, comp_entries;
  int size;
  
  char request[BUF_SIZE];
  char temp[BUF_SIZE+1];
  char buffer[BUF_SIZE+1];
  for(int i = 1; i < num_ports; i++){
    // poll servers
    if(-1 != (connection = create_client_socket(ports[i]))){
      sprintf(request, "GET /healthcheck HTTP/1.1\r\nHost: localhost:%d\r\n\r\n", ports[i]);

      send(connection, request, strlen(request), 0);

      size = recv(connection, temp, BUF_SIZE, 0);

      temp[size] = '\0';

      strncpy(buffer, temp, size+1);

      char* temp1 = buffer;

      char* token = strtok_r(temp, "\n", &temp1);
      int count = 0;

      while(count < 6 && token != NULL){
        if(count == 4){
          sscanf(token, "%d", &errors);
        }

        if(count == 5){
          sscanf(token, "%d", &entries);
        }

        count++;
        token = strtok_r(NULL, "\n", &temp1);
      }

      if(count < 6){
        size = recv(connection, temp, BUF_SIZE, 0);
        temp[size] = '\0';
        strncpy(buffer, temp, size);

        char* save = buffer;
        char* token2 = strtok_r(temp, "\n", &save);
        
        sscanf(token2, "%d", &errors);
        token2 = strtok_r(NULL, "\n", &save);
        sscanf(token2, "%d", &entries);
      }

      while(size == BUF_SIZE){
        size = recv(connection, temp, BUF_SIZE, 0);
      }
      
      close(connection);

      if(i != 1){
        if(entries <= comp_entries){
          if(entries == comp_entries){
            if(errors <= comp_errors){
              comp_entries = entries;
              comp_errors = errors;
            } else {
              moveFront(ordered_ports);
              insertAfter(ordered_ports, ports[i]);
            }

          } else {
            prepend(ordered_ports, ports[i]);
            comp_entries = entries;
            comp_errors = errors;
          } 
        
        } else {
          append(ordered_ports, ports[i]);
        }
      } else {
        prepend(ordered_ports, ports[i]);
        comp_entries = entries;
        if(entries == 0){
          comp_errors = 0;
        } else{
          comp_errors = errors;
        }
      }

    }
  } 
  
}


int send_cached(char* filename, int connfd, uint16_t port){
  char* response;
  char* look_result;
  int len = -1;
  int val = 2;
  int size2;
  int malloc_success = 0;

  pthread_mutex_lock(&cache_mutex);

  look_result = lookup(response_cache, filename, &len);
  
  if(look_result != NULL && len > 0){
    response = malloc(sizeof(char)*(len+1));
    memset(response, '\0', len+1);
    if(response == NULL){
      len = -1;
    } else {
      malloc_success = 1;
      memcpy(response, look_result, len); 
    }
  } else {
    len = -1;
  }

  pthread_mutex_unlock(&cache_mutex);
  
  if(response != NULL && len != -1){
    char request[BIG_BUF_SIZE];
    char reply_to_head[BUF_SIZE];
    int connection;
    sprintf(request, "HEAD /%s HTTP/1.1\r\nHost: localhost:%d\r\n\r\n", filename, port);

    pthread_mutex_lock(&get_mutex);
    connection = create_client_socket(port);
    pthread_mutex_unlock(&get_mutex);

    if(-1 != connection){

      int send_val = send(connection, request, strlen(request), 0);

      if(send_val == -1){
        val = 0;
      } else {
        size2 = recv(connection, reply_to_head, BUF_SIZE, 0);
        if(size2 <= 0){
          val = 0;
        }
      }

      close(connection);
    
    } else {
      val = 0;
    }

    if(val == 2){
      // time comparators
      uint64_t cached_time;
      uint64_t cached_time_r;
      
      int done = 0; // strtok flag
      
      // for cached response
      char temp[len+1];
      char temp2[len+1];
      char temp3[HEAD_SIZE];
      
      memcpy(temp, response, len);
      memcpy(temp2, response, len);
      
      temp[len] = '\0';
      temp2[len] = '\0';

      // for head response
      char temp_r[size2+1];
      char temp2_r[size2+1];
      char temp3_r[HEAD_SIZE];
      
      memcpy(temp_r, reply_to_head, size2);
      memcpy(temp2_r, reply_to_head, size2);
      
      temp_r[size2] = '\0';
      temp2_r[size2] = '\0';

      char* temp1 = temp2;
      char* token = strtok_r(temp, "\n", &temp1);
      int count = 0;

      char* temp1_r = temp2_r;
      char* token_r = strtok_r(temp_r, "\n", &temp1_r);

      while(done == 0 && token != NULL && token_r != NULL){

        if(count >= 2){
          if((sscanf(token, "Last-Modified: %[^\r\n]", temp3) == 1) && 
            (sscanf(token_r, "Last-Modified: %[^\r\n]", temp3_r) == 1)){
            done = 1;
          }
        }

        count++;
        token = strtok_r(NULL, "\n", &temp1);
        token_r = strtok_r(NULL, "\n", &temp1_r);
      }

      if(done == 1){
        struct tm tm0;
        struct tm tm1;
        memset(&tm0, 0, sizeof(tm0));
        memset(&tm1, 0, sizeof(tm1));
        
        if((strptime(temp3, "%a, %d %B %Y %H:%M:%S GMT", &tm0) != NULL) && 
          (strptime(temp3_r, "%a, %d %B %Y %H:%M:%S GMT", &tm1) != NULL)){
          cached_time = (uint64_t)timegm(&tm0);
          cached_time_r = (uint64_t)timegm(&tm1);
        } else {
          val = 0;
        }    
      } else {
        val = 0;
      }

      if(cached_time_r <= cached_time){
        send(connfd, response, len, 0);
        val = 1;
      } else {
        val = 0;
      }
      
    } else {
      val = 0;
    }
  } else {
    val = 0;
  }

  if((response != NULL) && (malloc_success == 1)){
    free(response);
    response = NULL;
  }
  return val;
}

void put_cache(char* cache_string, char* filename, int r_size){
  pthread_mutex_lock(&cache_mutex);
  insert(response_cache, filename, cache_string, r_size);
  
  if(size(response_cache) >= cache_size){
    delete_item(response_cache, replace_order[replace]);
  }

  memset(replace_order[replace], '\0', file_capacity);
  strcpy(replace_order[replace], filename);

  if(replace == (cache_size-1)){
    replace = 0;
  } else {
    replace++;
  }

  pthread_mutex_unlock(&cache_mutex);
}

void mark_bad_port(uint16_t port){
  int list_index = 0;
  int found_flag = 0;
  
  pthread_mutex_lock(&get_mutex);
  
  moveFront(ordered_ports);
  
  while((list_index < length(ordered_ports)) && found_flag == 0){
    if(get(ordered_ports) == port){
      set(ordered_ports, 0);
      found_flag = 1;
    }

    list_index++;
    moveNext(ordered_ports);
  }
  pthread_mutex_unlock(&get_mutex);
}


void handle_connection(int connfd, uint16_t port){
  // some response headers
  char br[] = "HTTP/1.1 400 Bad Request\r\nContent-Length: 12\r\n\r\nBad Request\n";
  char ni[] = "HTTP/1.1 501 Not Implemented\r\nContent-Length: 16\r\n\r\nNot Implemented\n";
  char ie[] = "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 22\r\n\r\nInternal Server Error\n";

  char req_type[BUF_SIZE];
  char file_name[BUF_SIZE];
  char host_name[BUF_SIZE];
  char buffer[BUF_SIZE];
  char request[BIG_BUF_SIZE];

  memset(buffer, '\0', BUF_SIZE);
  memset(request, '\0', BIG_BUF_SIZE);
  memset(req_type, '\0', BUF_SIZE);
  memset(file_name, '\0', BUF_SIZE);
  memset(host_name, '\0', BUF_SIZE);

  int need_req = 1;
  int size;
  int connection;

  size = recv(connfd, buffer, BUF_SIZE, 0); // recieve header

  if(need_req == 1 && size > 0){

    if(-1 == (need_req = get_data(buffer, req_type, file_name, host_name))){
      // the above line parses the header and checks that all the needed information is there
      send(connfd, br, strlen(br), 0);
      close(connfd);   
    } else {
      if(strcmp(req_type, "GET") == 0 && port != 0){ // request if GET
        if(check_valid_resource(file_name) == -1){
          // ensure that the given file name is valid
          send(connfd, br, strlen(br), 0);
          close(connfd);
          need_req = 1;
        } 

        if(need_req == 0){
          need_req = 1;

          int not_cached = -1;

          if(cache_size > 0 && file_capacity > 0){ // determine if requested file exists in cache
            not_cached = send_cached(file_name, connfd, port);
          } 


          if(not_cached <= 0){
            // assemble request header
            sprintf(request, "GET /%s HTTP/1.1\r\nHost: %s:%d\r\n\r\n", file_name, host_name, port);

            pthread_mutex_lock(&get_mutex);
            // connection IDs must be unique
            connection = create_client_socket(port);
            pthread_mutex_unlock(&get_mutex);

            if(-1 != connection){

              int send_val = send(connection, request, strlen(request), 0);

              if(send_val == -1){
                send(connfd, ie, strlen(ie), 0);
                close(connfd);
                close(connection);
                mark_bad_port(port);
              } else {

                int size2;
                int size_counter = 0;

                char response_buffer[BUF_SIZE];
                memset(response_buffer, '\0', BUF_SIZE);

                size2 = recv(connection, response_buffer, BUF_SIZE, 0);

                if(size2 == -1){
                  send(connfd, br, strlen(br), 0);
                  close(connfd);
                  close(connection);
                } else{
                  send(connfd, response_buffer, size2, 0);

                  char temp[size2+1];
                  char temp2[size2+1];
                  
                  memcpy(temp, response_buffer, size2);
                  memcpy(temp2, response_buffer, size2);
                  
                  temp[size2] = '\0';
                  temp2[size2] = '\0';
                  
                  char* temp1 = temp2;
                  char* token = strtok_r(temp, "\n", &temp1);
                  int count = 0;
                  int head_len = 0;
                  int cont_len;

                  while(count < 4 && token != NULL){
                    head_len += (strlen(token) + 1);
                    if(count == 1){
                      sscanf(token, "Content-Length: %d", &cont_len);
                    }

                    count++;
                    token = strtok_r(NULL, "\n", &temp1);
                  }

                  char* send_to_cache = malloc(size2+file_capacity+1);

                  if(not_cached == 0 && (cont_len <= file_capacity)){
                    memcpy(&send_to_cache[size_counter], response_buffer, size2);
                    size_counter += size2;
                  } else {
                    not_cached = -1;
                  }

                  int bytes_read;

                  if(size2 == BUF_SIZE){
                    bytes_read = size2;
                  } else {
                    bytes_read = 0;
                  }
                  
                  if(((size2 - head_len) != cont_len) || (((cont_len + size2) > BUF_SIZE) && size2 == BUF_SIZE)){

                    int counter = 0;

                    while(size2 > 0 && counter < (cont_len - bytes_read)){ // counter < cont_len
                      memset(buffer, '\0', BUF_SIZE);
                      size2 = recv(connection, response_buffer, BUF_SIZE, 0);

                      if(size2 > 0){
                        send(connfd, response_buffer, size2, 0);
                      }
                      
                      if(not_cached == 0){
                        memcpy(&send_to_cache[size_counter], response_buffer, size2);
                        size_counter += size2;
                      } 

                      counter += size2;
                    }
                    
                  }

                  if(not_cached == 0){
                    
                    if(size_counter > 0){
                      put_cache(send_to_cache, file_name, size_counter);  
                    } else {
                      send(connfd, ie, strlen(ie), 0);
                      close(connfd);
                    }
                    
                    
                    if(send_to_cache != NULL){
                      free(send_to_cache);
                      send_to_cache = NULL;
                    }
                  }
                  close(connection);
                }
              }
              
            } else {
              send(connfd, ie, strlen(ie), 0);
              close(connfd);
              mark_bad_port(port);
            }

          }

        }  

      } else {
        if(port == 0){
          send(connfd, ie, strlen(ie), 0);
          close(connfd);
          mark_bad_port(port);
        } else {
          send(connfd, ni, strlen(ni), 0);
          close(connfd);
        }
      }
    }
  }

}

void port_work(int connfd){
  pthread_mutex_lock(&get_mutex);

  if((num_requests == poll_freq) || (start == 0 && length(ordered_ports) == 0)){
    start = 1;
    num_requests = 0;
    probe_servers();
  }

  uint16_t port;
  int length_of_port_list = length(ordered_ports);

  if(length_of_port_list > 1){
    port = front(ordered_ports);
    
    if(port == 0){
      deleteFront(ordered_ports);
      port = front(ordered_ports);
    } else {
      deleteFront(ordered_ports);
      append(ordered_ports, port);
    }
  } else {
    if(length_of_port_list == 1){
      port = front(ordered_ports);
    } else {
      port = 0;
    }
  } 
  
  num_requests++;

  pthread_mutex_unlock(&get_mutex);
  handle_connection(connfd, port);
}


/*
* worker thread pool 
* dispatching function
*/
void* thread_work(){
  for(;;){

    pthread_mutex_lock(&connfd_mutex);
    while(length(work_list) < 1){
      pthread_cond_wait(&no_data, &connfd_mutex);
    }
    pthread_mutex_unlock(&connfd_mutex);

    pthread_mutex_lock(&handle_mutex);
    int connfd = (int)back(work_list);
    deleteBack(work_list);
    pthread_mutex_unlock(&handle_mutex);

    port_work(connfd);
  }  
}


/*
set up the HTTPserver connection at address listenfd and port port
*/
int main(int argc, char *argv[]) {
  int listenfd;
  int opt;
  int num_threads = DEFAULT_THREADS;
  int optcount = 0;
  uint16_t put_port;

  // initialize global variables
  poll_freq = DEFAULT_THREADS;
  cache_size = DEFAULT_SIZE;
  file_capacity = DEFAULT_FILE;
  num_ports = 0;
  num_requests = 0;
  replace = 0;
  start = 0;

  // allocate memoy for array of ports 
  // from argv
  ports = malloc(argc*sizeof(uint16_t));

  while(-1 != (opt = getopt(argc, argv, "-:N:R:s:m:"))){
    switch(opt){
      case 'N': // define number of threads
        num_threads = atoi(optarg);

        if(num_threads <= 0){
          num_threads = DEFAULT_THREADS;
        }
        break;

      case 'R': // define frequency of polling servers
        poll_freq = atoi(optarg);

        if(poll_freq < 0){
          poll_freq = DEFAULT_THREADS;
        }
        break;

      case 's':  // define size of cache
        cache_size = atoi(optarg);

        if(cache_size < 0){
          cache_size = DEFAULT_SIZE;
        }

        break;

      case 'm': // define max size of file stored in cache
        file_capacity = atoi(optarg);

        if(file_capacity < 0){
          file_capacity = DEFAULT_FILE;
        }

        break;  

      case ':':

        break;

      case 1:

        put_port = strtouint16(optarg);

        // ensure ports are valid, disregard duplicates
        if((optcount > 0) && (put_port > 0)){
          if(put_port != ports[0]){
            ports[optcount] = put_port;
            optcount++;
          }
        } else {
          ports[optcount] = put_port;
          optcount++;
        }

        num_ports++;

        break;
    }
  }

  if (optcount < 2) {
    errx(EXIT_FAILURE, "ERROR: not enough ports numbers specified");
  }

  if (argc < 2) {
    errx(EXIT_FAILURE, "wrong arguments");
  }

  if(cache_size > 0 && file_capacity > 0){ // allocate cache replacement order array
    replace_order = malloc(cache_size*sizeof(char*));
    for (int i = 0 ; i < cache_size; i++) {
        replace_order[i] = malloc(sizeof(char)*file_capacity);
    }
  }

  
  listenfd = create_listen_socket(ports[0]);

  pthread_t p_thread[num_threads]; 

  int connfd = 0;

  ordered_ports = newList(); // list of ports
  response_cache = newDictionary(1);

  probe_servers();

  for(int q = 0; q < num_threads; q++){
    pthread_create(&p_thread[q], NULL, thread_work, NULL);
  }

  work_list = newList();
  

  while(1) {
    pthread_mutex_lock(&connfd_mutex);
      
    if(length(work_list) > 0){
      pthread_cond_signal(&no_data);
    }

    pthread_mutex_unlock(&connfd_mutex);


    connfd = accept(listenfd, NULL, NULL); 
    // not atomic, surround with mutex

    if (connfd < 0) {
      warn("accept error");
      continue;
    
    } else {
      pthread_mutex_lock(&handle_mutex);
      prepend(work_list, (uint16_t)connfd);
      pthread_mutex_unlock(&handle_mutex);
      continue;
    }

    
  }  

  freeList(&work_list);
  freeList(&ordered_ports);
  freeDictionary(&response_cache);
  return EXIT_SUCCESS;
}

