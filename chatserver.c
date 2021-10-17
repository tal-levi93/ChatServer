#include  <stdio.h>
#include  <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>     	         
#include <netinet/in.h>    	         
#include <sys/socket.h>   	      
#include <netdb.h> 
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h> 
#include <errno.h>
#include <sys/select.h>
#include <sys/param.h>
#include <sys/select.h>
#include <signal.h>
#define MAX_BUFFER_SIZE 4096
#define GUEST "guest"

typedef struct msg_queue{
    char* msg_to_write;
    int to_fd;
    char* name;
    struct msg_queue* next;

}msg_queue;

msg_queue *head;
int welcome_socket;

void dequeue(msg_queue* node){
    
    msg_queue* curr = head;
    msg_queue* temp = NULL;
    msg_queue* previous = NULL;
    while(curr != NULL){
        if(curr == node){
            break;
        }
        previous = curr;
        curr = curr->next;
    }
    if(curr == head){
        temp = curr;
        head = curr->next;
    }
    else
    {
        temp = curr;
        previous->next = curr->next;
    }
    if(temp != NULL){
        free(temp->msg_to_write);
        free(temp->name);
        free(temp);
        
    }
    
    
};
void signal_handler(int sig_num){
    
    if(head != NULL){
        msg_queue* curr = head;
        msg_queue* temp;

        while(curr != NULL){
            temp = curr;
            curr = curr->next;
            free(temp->name);
            free(temp->msg_to_write);
            free(temp);            
        }
    }
    close(welcome_socket);
    exit(EXIT_SUCCESS);
}




int main(int argc , char* argv[]){
    signal(SIGINT , signal_handler);
    if(argc != 2){
        fprintf(stderr , "Usage: ./server <port>\n");
        exit(EXIT_FAILURE);
    }
    int port = atoi(argv[1]);
    if(port == 0){
        fprintf(stderr , "Usage: ./server <port>\n");
        exit(EXIT_FAILURE);
    }
    //-----------------------------------welcome socket-------write(fd , "HELLO" , sizeof("HELLO")+1);
                

    
    struct sockaddr_in serv_addr;
    welcome_socket = socket(AF_INET , SOCK_STREAM , 0);
    if(welcome_socket < 0){
        perror("Error in openning socket");
    }
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);
    if(bind(welcome_socket , (struct sockaddr *)&serv_addr , sizeof(serv_addr)) < 0){
        perror("error in binding");
        exit(EXIT_FAILURE);
    }
    listen(welcome_socket , 10);
    //-----------------------------------------------------------
    head = NULL;
    int maxfd = welcome_socket;
    fd_set main_fd_set;
    fd_set read_cpy;
    fd_set write_cpy;
    FD_ZERO(&main_fd_set);
    FD_SET(welcome_socket , &main_fd_set);
    int new_client_socket;
    int rc;
    int fd;
    int fd_2;
    char buf[MAX_BUFFER_SIZE];
    int queue_size = 0;
    while(1){
        if(queue_size == 0){// means we dont have msgs to read.
            FD_ZERO(&write_cpy);
        }
        else
        {
            write_cpy = main_fd_set;
        }
        read_cpy = main_fd_set;
        select(maxfd+1 , &read_cpy , &write_cpy , NULL , NULL);


        if(FD_ISSET(welcome_socket , &read_cpy)){
            
            new_client_socket = accept(welcome_socket , NULL , NULL);
            if(new_client_socket >= 0){
                FD_SET(new_client_socket , &main_fd_set);
                if(new_client_socket > maxfd){//maintain maxfd.
                    maxfd = new_client_socket;
                }
            }
        }
        if(queue_size > 0){
                for(fd = welcome_socket+1 ; fd<maxfd+1 ; fd++){
                    if(FD_ISSET(fd , &write_cpy)){
                        msg_queue* curr = head;
                        while(curr != NULL){
                            if(curr->to_fd == fd){
                                printf("Server is ready to write to socket %d\n" , fd);
                                write(fd , curr->msg_to_write ,strlen(curr->msg_to_write));
                                dequeue(curr);
                                queue_size--;
                                FD_CLR(fd , &write_cpy);
                                break;
                            }
                            curr = curr->next;
                        }
                    }
        
                }
        }
        for(fd = welcome_socket+1 ; fd<maxfd+1 ; fd++){
            if(FD_ISSET(fd , &read_cpy)){
                rc = read(fd , buf  , MAX_BUFFER_SIZE);
                buf[rc] = '\0';
                if(rc == 0){
                    close(fd);
                    FD_CLR(fd , &read_cpy);
                }
                else
                {
                    printf("Server is ready to read from socket %d\n" , fd);
                    for(fd_2 = welcome_socket+1 ; fd_2<maxfd+1 ; fd_2++){
                        if(fd_2 != fd && FD_ISSET(fd_2 , &main_fd_set)){
                            msg_queue* new_msg = (msg_queue*)malloc(sizeof(msg_queue));
                            char sock_fd[10];
                            sprintf(sock_fd, "%d", fd);
                            new_msg->name = (char*)malloc(sizeof(char)*(strlen("guest<>:")+strlen(sock_fd)+1));
                            strcpy(new_msg->name , GUEST);
                            strcat(new_msg->name , sock_fd);
                            strcat(new_msg->name , ":");
                            new_msg->msg_to_write = (char*)malloc(sizeof(char)*(strlen(new_msg->name)+strlen(buf)+1+strlen("\r\n")+1));
                            strcpy(new_msg->msg_to_write , new_msg->name);
                            strcat(new_msg->msg_to_write , buf);
                            strcat(new_msg->msg_to_write , "\r\n");
                            new_msg->to_fd = fd_2;
                            if(head == NULL){// there is no new msgs in queue;
                                head = new_msg;
                                new_msg->next = NULL;
                            } 
                            else
                            {
                                msg_queue* tmp;
                                tmp = head;
                                while(tmp->next != NULL){
                                    tmp = tmp->next;
                                }
                                tmp->next = new_msg;
                                new_msg->next = NULL;
                            }
                            queue_size++;
                        }
                        continue;
                    }
                }
            }
        }
    }


    







    return 0;
};

