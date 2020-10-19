/*
** modified from http://beej.us/guide/bgnet/examples/listener.c
** talker.c -- a datagram "client" demo
other resources: https://www.geeksforgeeks.org/udp-server-client-implementation-c/
IP: 128.100.13.153
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <stdbool.h>
#include <math.h>
#include "packet.h"

#define MAXBUFLEN 65535


// helper
char* pacToStr(packet pac){
    char *result = malloc(1200*sizeof(char));

    sprintf(result, "%d", pac.total_frag);
    memcpy(result+strlen(result), ";", sizeof(char));

    sprintf(result+strlen(result), "%d", pac.frag_no);
    memcpy(result+strlen(result), ";", sizeof(char));

    sprintf(result+strlen(result), "%d", pac.size);
    memcpy(result+strlen(result), ";", sizeof(char));

    sprintf(result+strlen(result), "%s", pac.filename);
    memcpy(result+strlen(result), ";", sizeof(char));

    memcpy(result+strlen(result), pac.filedata, sizeof(char)*1000);
    // exit(0);
    // printf("%d", strlen(result));
    return result;

    // char total_frag[10];
    // char frag_no[10];
    // char size[10];
    // sprintf(total_frag, "%d", pac.total_frag);
    // strcpy(result, total_frag);
    // strcat(result, ";");
    // sprintf(frag_no, "%d", pac.frag_no);
    // strcat(result, frag_no);
    // strcat(result, ";");
    // sprintf(size, "%d", pac.size);
    // strcat(result, size);
    // strcat(result, ";");
    // strcat(result, pac.filename);
    // strcat(result, ";");
    // strcat(result, pac.filedata);
    // return result;
}

void slice_str(const char * str, char * buffer, size_t start, size_t end) {
    size_t j = 0;
    for ( size_t i = start; i <= end; ++i ) {
        buffer[j++] = str[i];
    }
    buffer[j] = 0;
}

// #define SERVERPORT "4950" // the port users will be connecting to
int main(int argc, char *argv[]){
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int numbytes;
    if (argc != 3) {
        fprintf(stderr,"Error: please input <server address> <server port number>\n");
        exit(1);
    }


    // Sets the first num bytes of the block of memory pointed by ptr to the specified value (interpreted as an unsigned char).
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // ip4 or 6
    hints.ai_socktype = SOCK_DGRAM; // type
    // int getaddrinfo(const char *node, // e.g. "www.example.com" or IP
    // const char *service, // e.g. "http" or port number
    // const struct addrinfo *hints, // carries addr information
    // struct addrinfo **res);
    if ((rv = getaddrinfo(argv[1], argv[2], &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }
    
    // loop through all the results and make a socket
    for(p = servinfo; p != NULL; p = p->ai_next) {
        // try to make a socket on the first sucessful one
        // return file descriptor #
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("deliver: socket");
            continue;
        }
        break;
    }
    if (p == NULL) {
        fprintf(stderr, "talker: failed to create socket\n");
        return 2;
    }

    // start type in ftp command
    char command[MAXBUFLEN]; 
    fgets(command, MAXBUFLEN, stdin); 
    // split
    char *token = strtok(command, " "); 
    if(strcmp(token, "ftp")!=0){
        printf("please enter: ftp <file name>\n");
    }
    token = strtok(NULL, " ");
    if(token==NULL){ // end of string
        printf("not enough argument\n");
        exit(1);
    }
    char filename[MAXBUFLEN];
    strcpy(filename, token);
    printf("looking for file: %s", filename);

    // trim filename
    char *tmp = filename;
    int len = strlen(tmp);

    while(isspace(tmp[len-1])) tmp[--len] = 0;
    while(*tmp && isspace(* tmp)) ++tmp, --len;

    memmove(filename, tmp, len+1);

    if(access(filename, F_OK) == -1){
        printf("no such file in the directory\n");
        exit(1);
    }

    // measure time
    clock_t t = clock();

    //send "ftp" to server
    if ((numbytes = sendto(sockfd, "ftp", strlen("ftp"), 0, p->ai_addr, p->ai_addrlen)) == -1) {
        perror("deliver: sendto");
        exit(1);
    }
    // receive from server...
    char buf[MAXBUFLEN];
    socklen_t addr_len=sizeof(p->ai_addr);
    if (
        (numbytes = recvfrom(sockfd, buf, MAXBUFLEN-1 , 0, (struct sockaddr *) &p->ai_addr, (unsigned int * restrict) &addr_len)) == -1
        ) {
		perror("recvfrom");
		exit(1);
	}
    printf("listener: packet contains \"%s\"\n", buf);

    if (strcmp(buf, "yes")==0){
        printf("a file transfer can start\n");
    } else{
        printf("Error: didn't receive yes from server\n");
        exit(1);
    }

    // measure time
    t = clock() - t;
    printf("The round trip took %fms\n", (double)t);

    /* lab3: transfer the actual file */
    FILE *f=fopen(filename,"r");
    fseek(f, 0, SEEK_END); // seek to end of file
    int fsize = ftell(f); // get current file pointer
    fseek(f, 0, SEEK_SET); // seek back to beginning of file
    printf("fileLen: %d\n", fsize);
    int num_frag= floor((float)(fsize/1000))+1; //number of fragments needed
    int frag_num=1;

    char data[fsize];
    fread(data, sizeof(char), fsize, f);

    while(1){

        //fill in packet information
        packet pac;
        pac.total_frag=num_frag;
        pac.frag_no=frag_num;
        if(pac.frag_no==pac.total_frag){ //last one
            pac.size=fsize%1000;
        }else{
            pac.size=1000;
        }
        slice_str(data, pac.filedata, (pac.frag_no-1)*1000, (pac.frag_no-1)*1000+pac.size-1);
        // printf("%d\n", strlen(pac.filedata));
        // printf("%s\n", pac.filedata);

        pac.filename = (char*)malloc(sizeof(char*)*strlen(filename));
        strcpy(pac.filename, filename);

        // test packet properties
        // printf("%s, %s, %i, %i, %i", pac.filedata, pac.filename, pac.size, pac.total_frag, pac.frag_no);
        // printf("\n");
        // exit(1);

        //convert package into a string
        // printf("%i, %i, %i", strlen(pac.filedata), pac.total_frag, pac.frag_no); // test
        char *pacStr=pacToStr(pac);
        printf("%d\n", strlen(pacStr));

        // sent to server
        if((numbytes = sendto(sockfd, pacStr, strlen(pacStr), 0 , (struct sockaddr *)&p->ai_addr, p->ai_addrlen)) == -1) {
            printf("error for sending packet\n");
            exit(1);
        }
        
        // receive from server a "ACK", for each package
        if (
            (numbytes = recvfrom(sockfd, buf, MAXBUFLEN-1 , 0, (struct sockaddr *) &p->ai_addr, (unsigned int * restrict) &addr_len)) == -1
        ) {
            perror("recvfrom");
            exit(1);
        }
        // printf("listener: packet contains \"%s\"\n", buf);
        if (strcmp(buf, "ACK")==0){
            printf("fragment ack received\n");
        } else{
            printf("Error: didn't receive yes from server for the fragment\n");
            exit(1);
        }

        // last one sent and received, break
        if (frag_num==num_frag){
            break;
        }
        // add 1, continue
        frag_num=frag_num+1;
    }

    /* close */
    freeaddrinfo(servinfo);
    // printf("deliver: sent %d bytes to %s\n", numbytes, argv[1]);
    close(sockfd);

    fclose(f);
    
    return 0;
}