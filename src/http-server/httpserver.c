#include <err.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <getopt.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/file.h>
#include "List.h"

pthread_mutex_t connfd_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t handle_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t no_data;

#define BUF_SIZE 1000
#define BIG_BUF_SIZE 2500

int log_true;
uint16_t port;
char log_file_name[20];

List work_list;


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

// verify log file format is correct
int check_format(int* num_entries, int* num_errs){
  pthread_mutex_lock(&log_mutex);
  int return_val = 1;
  int fd;
  int entries = 0;
  int errs = 0;
  
  if(0 <= (fd = open(log_file_name, O_RDONLY))){
    char buffs[BIG_BUF_SIZE];
    char* buff = buffs;
    char tokens[BIG_BUF_SIZE];
    char* token = tokens;
    char tokens2[BIG_BUF_SIZE];
    char* token2 = tokens2;
    char *save, *save2;

    while(read(fd, buff, BIG_BUF_SIZE) > 0){
      token = strtok_r(buff, "\n", &save);

      if(token == NULL){
        return_val = -1;
      }

      while(token != NULL){
        entries++;
        int tab_count = 0;
        int token_found = 0;
        char temp[BIG_BUF_SIZE];
        strcpy(temp, token);
        token2 = strtok_r(token, "\t", &save2);

        while(token2 != NULL){
          if(strcmp("FAIL",token2) == 0){
            errs++;
          }

          if(strcmp(temp,token2) != 0){
            token_found = 1;
            if(tab_count > 4){
              return_val = -1;
            }

            tab_count++;
          } else {
            entries--;
          }
          
          token2 = strtok_r(NULL, "\t", &save2);
        }

        if(tab_count < 2 && token_found == 1){ 
          return_val = -1;
        }

        token = strtok_r(NULL, "\n", &save);
      }

    }

    if(num_entries && num_errs != NULL){
      *num_entries = entries;
      *num_errs = errs;
    }

    return_val = 1;

  } else {
    printf("ERROR Opening Log File");
    return_val = -1;
  }

  close(fd);
  pthread_mutex_unlock(&log_mutex);
  return return_val;
}


/*
parses http request header
returns -1 if error/ not all required elements are found
returns 0 if all information is found
*/
int get_data(char* buffer, char* req_type, char* file_name, char* host_name, int* cont_len){
  char** save = &buffer;
  char* token = strtok_r(buffer, " ", save);
  int val = 0;

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
      if(sscanf(token, "Host: %s", host_name) == 1){
        if(strcmp(req_type, "PUT") != 0){
          return 0;
        } else {
          val++;
        }
      }
    }

    if(val == 3){
      if(sscanf(token, "Content-Length: %d", cont_len) == 1){
        if(strcmp(req_type, "PUT") != 0){
          return -1;
        } else {
          return 0;
        }
      }
    }
    
    token = strtok_r(NULL, "\r\n", save);
  }

  return -1;
}

void write_log(char* log_head, char* len, char* for_log, int fail){
  char* new_line = "\n\0";
  int log_file;

  pthread_mutex_lock(&log_mutex);
  if(0 <= (log_file = open(log_file_name, O_RDWR | O_APPEND))){
    write(log_file, log_head, strlen(log_head));
    
    if(fail == 0){
      write(log_file, len, strlen(len));
      
      if(for_log != NULL){
        write(log_file, for_log, strlen(for_log));
      }
    }
    if(fail > 0){
      char num[BUF_SIZE/10];
      sprintf(num, "%d", fail);
      write(log_file, num, strlen(num));
    }
    write(log_file, new_line, strlen(new_line));
    close(log_file);
  } else {
    printf("ERROR Opening Log File");
  }
  pthread_mutex_unlock(&log_mutex);

}


void handle_connection(int connfd){
  // some response headers
  char ok[] = "HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\nOK\n";
  char cr[] = "HTTP/1.1 201 Created\r\nContent-Length: 8\r\n\r\nCreated\n";
  char br[] = "HTTP/1.1 400 Bad Request\r\nContent-Length: 12\r\n\r\nBad Request\n";
  char fb[] = "HTTP/1.1 403 Forbidden\r\nContent-Length: 10\r\n\r\nForbidden\r\n";
  char ff[] = "HTTP/1.1 404 File Not Found\r\nContent-Length: 15\r\n\r\nFile Not Found\n";
  char ie[] = "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 22\r\n\r\nInternal Server Error\n";
  char ni[] = "HTTP/1.1 501 Not Implemented\r\nContent-Length: 16\r\n\r\nNot Implemented\n";

  char fb_h[] = "HTTP/1.1 403 Forbidden\r\nContent-Length: 10\r\n\r\n";
  char ff_h[] = "HTTP/1.1 404 File Not Found\r\nContent-Length: 15\r\n\r\n";
  char ie_h[] = "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 22\r\n\r\n";
  char br_h[] = "HTTP/1.1 400 Bad Request\r\nContent-Length: 12\r\n\r\n";

  char req_type[BUF_SIZE];
  char file_name[BUF_SIZE];
  char host_name[BUF_SIZE];
  char log_head[BUF_SIZE];
  char for_log[(BUF_SIZE*2)+1];
  char buffer[BUF_SIZE];
  char log_fail[BUF_SIZE+10];
  char len_log[BUF_SIZE/2];
  
  int cont_len;
  int fd;
  int need_req = 1;
  int log_data = -1;
  int size;


  size = recv(connfd, buffer, BUF_SIZE, 0); // recieve header

  if(log_true > 0){
    char fail_buff[BUF_SIZE];
    strcpy(fail_buff,buffer);
    char* ptr;
    char* fail_token = strtok_r(fail_buff, "\r", &ptr);
    
    if(fail_token != NULL){
      snprintf(log_fail, BUF_SIZE+10, "FAIL\t%s\t", fail_token);
    } else {
      snprintf(log_fail, BUF_SIZE+10, "FAIL\t%s\t", buffer);
    }  
  }

  if(need_req == 1 && size > 0){

    if(-1 == (need_req = get_data(buffer, req_type, file_name, host_name, &cont_len))){
      // the above line parses the header and checks that all the needed information is there
      if(log_true > 0){
        write_log(log_fail, NULL, NULL, 400);
      }
      send(connfd, br, strlen(br), 0);
      close(connfd);
    
    } else {

      if(log_true > 0){
        char req[4];
        char fn[20];
        strcpy(req, req_type);
        strcpy(fn, file_name);
        sprintf(log_head, "%s\t/%s\tlocalhost:%d\t", req, fn, port);  
      }

      if(check_valid_resource(file_name) == -1){
        // ensure that the given file name is valid
        if(log_true > 0){
          write_log(log_fail, NULL, NULL, 400);
        }

        if(strcmp(req_type, "HEAD") == 0){
          send(connfd, br_h, strlen(br_h), 0);
          close(connfd);
        }else {
          send(connfd, br, strlen(br), 0);
          close(connfd);
        }
      } 
    }

  }

  if(strcmp(file_name, "healthcheck") == 0){
    if((strcmp(req_type, "GET") == 0) && (log_true > 0)){
      int responses;
      int errors;
      
      if(check_format(&responses, &errors) == -1){
        write_log(log_fail, NULL, NULL, 500);
        send(connfd, ie, strlen(ie), 0);
      } else {
        char bod[BUF_SIZE/2];
        sprintf(bod, "%d\n%d\n", errors, responses);
        char ok_gh[] = "HTTP/1.1 200 OK\r\nContent-Length: ";
        char temph[BUF_SIZE];
        char head_endh[] = "\r\n\r\n";
        sprintf(temph, "%s%lu%s%s", ok_gh, strlen(bod), head_endh, bod);
        // start from stack overflow:
        // https://stackoverflow.com/questions/46210513/
        // how-to-convert-a-string-to-hex-and-vice-versa-in-c
        for (unsigned long int i = 0, j = 0; i < strlen(bod); ++i, j += 2){
          sprintf(for_log + j, "%02x", bod[i] & 0xff);
        }
        // end from stack overflow
        sprintf(len_log, "%lu\t", strlen(bod));
        write_log(log_head, len_log, for_log, 0);

        send(connfd, temph, strlen(temph), 0);
      }

    } else{
      if(log_true > 0){
        write_log(log_fail, NULL, NULL, 403);
      }

      if(strcmp(req_type, "HEAD") == 0){
        send(connfd, fb_h, strlen(fb_h), 0);
      }

      else {
        send(connfd, fb, strlen(fb), 0);
      }
    }
    close(connfd);
  }

  else if(strcmp(req_type, "PUT") == 0){ // for put requests
    char cont[BUF_SIZE]; 
    int count = 0;
    int data_rcvd = 0;
    sprintf(len_log, "%d\t", cont_len);

    if(need_req == 0){
      need_req = 1;
      
      // open file if it already exists
      if(0 <= (fd = open(file_name, O_TRUNC | O_WRONLY))){
        if(flock(fd, LOCK_EX) == -1){
          if(log_true > 0){
            write_log(log_fail, NULL, NULL, 500);
          }
          send(connfd, ie, strlen(ie), 0);
          close(connfd);
        }

        if(cont_len > 0){
          
          while(count < cont_len){
            data_rcvd = recv(connfd, cont, BUF_SIZE, 0);

            if(data_rcvd == -1){
              if(log_true > 0){
                write_log(log_fail, NULL, NULL, 500);
              }
              send(connfd, ie, strlen(ie), 0);
              close(connfd);
            }

            if(log_true > 0 && log_data == -1){
              // start from stack overflow:
              // https://stackoverflow.com/questions/46210513/
              // how-to-convert-a-string-to-hex-and-vice-versa-in-c
              for (int i = 0, j = 0; i < data_rcvd; ++i, j += 2){
                sprintf(for_log + j, "%02x", cont[i] & 0xff);
              }
              // end from stack overflow
              log_data = data_rcvd;
            }

            int temp = write(fd, cont, data_rcvd);

            if(temp == -1){
              if(log_true > 0){
                write_log(log_fail, NULL, NULL, 500);
              }

              send(connfd, ie, strlen(ie), 0);
              close(connfd);
            }

            count += data_rcvd;
          }

          if(log_true > 0){
            write_log(log_head, len_log, for_log, 0);
          }

          send(connfd, ok, strlen(ok), 0);
        }

        flock(fd, LOCK_UN);
        close(fd);
        close(connfd);

      } else if(0 <= (fd = creat(file_name, S_IRWXU | O_WRONLY))) {
        if(flock(fd, LOCK_EX) == -1){
          if(log_true > 0){

            write_log(log_fail, NULL, NULL, 500);
          }
          send(connfd, ie, strlen(ie), 0);
          close(connfd);
        }

        if(cont_len > 0){

          while(count < cont_len){
            
            data_rcvd = recv(connfd, cont, BUF_SIZE, 0);
            
            if(data_rcvd == -1){
              if(log_true > 0){
                write_log(log_fail, NULL, NULL, 500);
              }

              send(connfd, ie, strlen(ie), 0);
              close(connfd);
            }

            if(log_true > 0 && log_data == -1){
              // start from stack overflow:
              // https://stackoverflow.com/questions/46210513/
              // how-to-convert-a-string-to-hex-and-vice-versa-in-c
              for (int i = 0, j = 0; i < data_rcvd; ++i, j += 2){
                sprintf(for_log + j, "%02x", cont[i] & 0xff);
              }
              // end from stack overflow
              log_data = data_rcvd;
            }

            int temp = write(fd, cont, data_rcvd);

            if(temp == -1){
              if(log_true > 0){
                write_log(log_fail, NULL, NULL, 500);
              }

              send(connfd, ie, strlen(ie), 0);
              close(connfd);
            }

            count += data_rcvd;
          }

          if(log_true > 0){
            write_log(log_head, len_log, for_log, 0);
          }

          send(connfd, cr, strlen(cr), 0);
        }

        flock(fd, LOCK_UN);
        close(fd);
        close(connfd);

      } else {
         //error
        if(log_true > 0){
          write_log(log_fail, NULL, NULL, 403);
        }
        send(connfd, fb, strlen(fb), 0);
        close(connfd);
      }
    }

  } // end of put

  // GET
  else if(strcmp(req_type, "GET") == 0){
    char data[BUF_SIZE];
    int size2;
    uint32_t i = 0;

    if(need_req == 0){
      need_req = 1;

      if(0 <= (fd = open(file_name, O_RDONLY))){
        
        while(0 < (size2 = read(fd, data, BUF_SIZE))){
          
          i += size2;
        } 
        
        if(data != NULL || i == 0){
          char send_data[BUF_SIZE];
          char ok_g[] = "HTTP/1.1 200 OK\r\nContent-Length: ";
          char temp[BUF_SIZE];
          sprintf(temp, "%d", i); // get the size of a string representing the content length
          int lngth = strlen(ok_g)+strlen(temp);
          char header[lngth];
          sprintf(header, "%s%d", ok_g, i);
          char head_end[] = "\r\n\r\n";
          // send off the data
          send(connfd, header, lngth, 0);
          send(connfd, head_end, strlen(head_end), 0);
          int size3;
          if(i > 0){
            if(0 <= (fd = open(file_name, O_RDONLY))){

              while(0 < (size3 = read(fd, send_data, BUF_SIZE))){
                send(connfd, send_data, size3, 0);

                if(log_true > 0 && log_data == -1){
                  // start from stack overflow:
                  // https://stackoverflow.com/questions/46210513/
                  // how-to-convert-a-string-to-hex-and-vice-versa-in-c
                  for (int ii = 0, j = 0; ii < size3; ++ii, j += 2){
                    sprintf(for_log + j, "%02x", send_data[ii] & 0xff);
                  }
                  // end from stack overflow
                  sprintf(len_log, "%d\t", i);
                  write_log(log_head, len_log, for_log, 0);
                  log_data = 1;
                }
              }

            } else {
              if(log_true > 0){
                write_log(log_fail, NULL, NULL, 400);
              }
              send(connfd, br, strlen(br), 0); // eaccess from errno
              close(connfd);
            }
          }

          close(fd);
          close(connfd);
              
        }
        else {
          if(log_true > 0){
            write_log(log_fail, NULL, NULL, 500);
          }
          send(connfd, ie, strlen(ie), 0);
          close(connfd);
        }

      } else {
        if(log_true > 0){
          write_log(log_fail, NULL, NULL, 404);
        }
        send(connfd, ff, strlen(ff), 0);
        close(connfd);
      }
    }  
  }

  else if(strcmp(req_type, "HEAD") == 0){ // identical to get, just dont send body
    char datas[BUF_SIZE];
    int size2;
    uint32_t size_count;

    if(need_req == 0){
      need_req = 1;

      if(0 <= (fd = open(file_name, O_RDONLY))){

        if(flock(fd, LOCK_SH) == -1){
          if(log_true > 0){
            write_log(log_fail, NULL, NULL, 500);
          }
          send(connfd, ie, strlen(ie), 0);
          close(connfd);
        }

        while(0 < (size2 = read(fd, datas, BUF_SIZE))){
          size_count += size2;
        } 

        sprintf(len_log, "%d\t", size_count);

        if(datas != NULL || size_count == 0){
            
            char ok_gh[] = "HTTP/1.1 200 OK\r\nContent-Length: ";
            char temph[BUF_SIZE];
            char head_endh[] = "\r\n\r\n";
            sprintf(temph, "%s%d%s", ok_gh, size_count, head_endh);
            send(connfd, temph, strlen(temph), 0);


            flock(fd, LOCK_UN);
            close(fd);

            if(log_true > 0){
              write_log(log_head, len_log, NULL, 0);
            }

            close(connfd);
              
        }
        else {
          if(log_true > 0){
            write_log(log_fail, NULL, NULL, 500);
          }
          send(connfd, ie_h, strlen(ie_h), 0);
          close(connfd);
        }

      } else {
        if(log_true > 0){
          write_log(log_fail, NULL, NULL, 404);
        }
        send(connfd, ff_h, strlen(ff_h), 0);
        close(connfd);
      }
    }
  } // end of head 


  else {
    if(log_true > 0){
      write_log(log_fail, NULL, NULL, 501);
    }
    send(connfd, ni, strlen(ni), 0);
    close(connfd);
  }

  close(connfd);  

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
    int connfd = back(work_list);
    deleteBack(work_list);
    pthread_mutex_unlock(&handle_mutex);

    handle_connection(connfd);

  }  
}


/*
set up the HTTPserver connection at address listenfd and port port
*/
int main(int argc, char *argv[]) {
  int listenfd;
  int opt;
  int num_threads = 0;
  log_true = 0; 
  int log_file;
  port = 0;

  while(-1 != (opt = getopt(argc, argv, "-:N:l:"))){
    switch(opt){
      case 'N':
        num_threads = atoi(optarg);
        break;

      case 'l': 

        if(0 <= (log_file = open(optarg, O_RDWR))){
          strcpy(log_file_name, optarg);
          log_true = 1;

          if(check_format(NULL, NULL) == -1){
            errx(EXIT_FAILURE, "ERROR: LOG FILE BAD FORMAT");
          }

          close(log_file);

        } else if(0 <= (log_file = creat(optarg, S_IRWXU | O_RDWR))){
          strcpy(log_file_name, optarg);
          log_true = 2;

          close(log_file);

        } else {
          errx(EXIT_FAILURE, "ERROR Opening Log File");
        }

        break;

      case ':':
        fprintf(stderr, "USAGE ERROR");
        exit(EXIT_FAILURE); 
        break;

      case 1:
        port = strtouint16(optarg);
        
        if (port == 0) {
          errx(EXIT_FAILURE, "invalid port number: %s", optarg);
        }
        break;
    }
  }

  if (port == 0) {
    errx(EXIT_FAILURE, "invalid port number: %d", port);
  }

  if (argc < 2) {
    printf("%d\n", argc);
    errx(EXIT_FAILURE, "wrong arguments: %d port_num", port);
  }

  if(num_threads == 0){
    num_threads = 5;
  }
  
  listenfd = create_listen_socket(port);

  pthread_t p_thread[num_threads]; 

  int connfd = 0;

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

    if (connfd < 0) {
      warn("accept error");
      continue;
    
    } else {
      pthread_mutex_lock(&handle_mutex);
      prepend(work_list, connfd);
      pthread_mutex_unlock(&handle_mutex);
      continue;
    }

    
  }  

  freeList(&work_list);
  return EXIT_SUCCESS;
}

