#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netgraph.h>
#include <netgraph/ng_ksocket.h>
#include <netinet/tcp.h>

void makeNgSocket( int *csock, int *dsock);
void makeKsocket(char *ip, char *port);
void sendData(int dsock, char *buf, char *ourhook);
void getSocketPeerName( int csock, char *peername );
void connectNg(char *ip, char *port);
void tcpNoDelay(char *nodeName);
void soRecvbuf(char *nodeName, size_t size);

int csock, dsock;

int main( int argc, char **argv ) {
    char *ip, *port;
 
    if ( argc != 3 ) {
        printf("arg count != 3\n");
        exit(1);
    }
    ip = argv[1];
    port = argv[2];

    printf("argv[1] = %s argv[2] = %s\n", argv[1], argv[2]);

    makeNgSocket(&csock, &dsock);
    makeKsocket(ip, port);
    while (1) {

    }
}

void makeNgSocket (int *csock, int *dsock) {
    char name[22];

   memset(name, 0, sizeof(name));
    sprintf(name, "hello-socket");
    if (NgMkSockNode(name, csock, dsock) < 0) {
        printf("Error: %s\n", strerror(errno));      
        exit(1);
    }
}

void makeKsocket( char *ip, char *port) {
    int errCode, token;
    u_char buf[200];
    char path[NG_PATHSIZ];
    struct sockaddr_in getname;
    char ourhook[NG_HOOKSIZ], idbuf[NG_HOOKSIZ];
    u_char new_buf[2048], *buf_alloc;
    int data, received_bytes = 0;
    char hook[NG_HOOKSIZ];
    FILE *fd;
   
    connectNg(ip, port);
   
    memset(buf, 0, sizeof(buf));
    fgets(buf, 200, stdin);
    //sprintf(buf, "GET / \n");
    
    memset(ourhook, 0, sizeof(ourhook));
    sprintf(ourhook, "left");
    sendData(dsock, buf, ourhook);
   
    fd = fopen("tmp.txt", "w");
    NgSetDebug(0);
    //while ( (data = NgRecvData( dsock, new_buf, (size_t )2047, hook )) > 0 ) {
    //    received_bytes += data;
    //    printf("Received new portion of data size = %d\n", data);
    //    fprintf(fd, new_buf);
    //}
    while ( (data = NgAllocRecvData( dsock, &buf_alloc,  hook )) > 0 ) {
        received_bytes += data;
        if (hook != "left") {
            printf("Received new portion of data size = %d strlen(buf_alloc) = %d\n", data, strlen(buf_alloc));
            fwrite(buf_alloc, data, 1, fd);
            //memset(buf_alloc, 0, data);
            free(buf_alloc);
            if (received_bytes >= 100000000) {
                fflush(fd);
            }
        } else {
            printf("Received unexpected data on hook %s\n", hook);
        }
    }
    fclose(fd);

    printf("Now data = %d Total bytes = %d\n", data, received_bytes);
}

void sendData ( int dsock, char *buf , char *ourhook) {
    int errCode;

    printf("Sending data to socket data_len = %d\n", strlen(buf));
    errCode = NgSendData( dsock, ourhook, buf, strlen(buf) );
    if ( errCode < 0 ) {
        printf("Error sending data to socket: %s\n", strerror(errno));
        exit(1);
    }
    printf("errCode = %d\n", errCode);
}

void getSocketPeerName (int csock, char *nodeName ) {
    int token;
    struct sockaddr_in *peername;
    struct ng_mesg *resp;
    token = NgSendMsg( csock, nodeName, NGM_KSOCKET_COOKIE, NGM_KSOCKET_GETPEERNAME, NULL, 0);

    if ((int) token == -1) {
        switch (errno) {
            case ENOTCONN:
                printf("%s : Socket not connected, node : will be shutdown\n", __FUNCTION__);
                return;
                break;
            case ENOENT: 
                printf("%s (): Node already closed\n", __FUNCTION__);
                return;
                break;
            default: 
                printf("%s (): An error has occured while getpeername from node: %s\n",
                    __FUNCTION__, strerror(errno));
                return;
                break;
        }
    }
    if (NgAllocRecvMsg(csock, &resp, NULL) < 0) {
        printf("Error receiving response: %s\n", strerror(errno));
        return;
    }

    peername = (struct sockaddr_in *)resp->data;
    printf("%s(): Peer %s:%d still connected\n",
            __FUNCTION__, inet_ntoa(peername->sin_addr), ntohs(peername->sin_port));
    free(resp);
} 

void connectNg (char *ip, char *port) {
    struct ngm_mkpeer mkp;
    struct ng_mesg *resp;
    struct sockaddr_in ip4addr;
    char ourhook[NG_HOOKSIZ], idbuf[NG_HOOKSIZ];
    char path[NG_PATHSIZ];
    int errCode, token;
    
    memset(path, 0, sizeof(path));
    sprintf(path, "."); 

    memset(ourhook, 0, sizeof(ourhook));
    sprintf(ourhook, "left");
 
    snprintf(mkp.type, sizeof(mkp.type), "ksocket");
    snprintf(mkp.ourhook, sizeof(mkp.ourhook), ourhook);
    snprintf(mkp.peerhook, sizeof(mkp.peerhook), "inet/stream/tcp");
    
    errCode = NgSendMsg( csock, path, NGM_GENERIC_COOKIE, NGM_MKPEER, &mkp, sizeof(mkp) );
    if ( errCode < 0 ) {
        printf("Error: %s\n", strerror(errno));
        exit(1);
    }

    sprintf(path, ".:left"); 
    NgNameNode( csock, path, "tcp_client_ksocket");
    
    ip4addr.sin_family = AF_INET;
    ip4addr.sin_port = htons(atoi(port));
    ip4addr.sin_len = sizeof(struct sockaddr_in);
    inet_pton(AF_INET, ip, &ip4addr.sin_addr);
 

    NgSetDebug(3); 
    tcpNoDelay("tcp_client_ksocket:");
    //soRecvbuf("tcp_client_ksocket:", 65535);
    errCode = NgSendMsg( csock, path, NGM_KSOCKET_COOKIE, NGM_KSOCKET_CONNECT,  &ip4addr, sizeof(ip4addr) );
    if ( token < 0 && errno != EINPROGRESS ) {
        printf("Error connecting to %s : %s\n", inet_ntoa(ip4addr.sin_addr), strerror(errno));
        //exit(1);
    }
    
    if ( NgAllocRecvMsg(csock, &resp, NULL) < 0 ) {
        printf("Error receiving response on ksocket_connect: %s\n", strerror(errno));
    } else {
        int i;
        printf("Rceived data int resp: %s\n", (char *)resp->data );
        free(resp);
    }
}

void tcpNoDelay (char *nodeName) {
    union {
        u_char buf[sizeof(struct ng_ksocket_sockopt) + sizeof(int)];
        struct ng_ksocket_sockopt sockopt;
    } sockopt_buf;
    struct ng_ksocket_sockopt * const sockopt = &sockopt_buf.sockopt;
    int one = 1;

    // setsockopt resolve TIME_WAIT problem
    // setsockopt(fd,SOL_SOCKET,SO_REUSEPORT,&one,sizeof(int)) < 0)
    memset(&sockopt_buf, 0, sizeof(sockopt_buf));

    sockopt->level = IPPROTO_TCP;
    sockopt->name = TCP_NODELAY;
    memcpy(sockopt->value, &one, sizeof(int));
    if (NgSendMsg(csock, nodeName, NGM_KSOCKET_COOKIE, NGM_KSOCKET_SETOPT,
            sockopt, sizeof(sockopt_buf)) == -1) {
        printf("%s(): Sockopt set failed : %s\n", __FUNCTION__,
                strerror(errno));
        return;
    }
    return;
}

void  soRecvbuf (char *nodeName, size_t size) {
    union {
        u_char buf[sizeof(struct ng_ksocket_sockopt) + sizeof(int)];
        struct ng_ksocket_sockopt sockopt;
    } sockopt_buf;
    struct ng_ksocket_sockopt * const sockopt = &sockopt_buf.sockopt;
    int one = 1;

    // setsockopt resolve TIME_WAIT problem
    // setsockopt(fd,SOL_SOCKET,SO_REUSEPORT,&one,sizeof(int)) < 0)
    memset(&sockopt_buf, 0, sizeof(sockopt_buf));

    sockopt->level = SOL_SOCKET;
    sockopt->name = SO_RCVBUF;
    memcpy(sockopt->value, &size, sizeof(size));
    if (NgSendMsg(csock, nodeName, NGM_KSOCKET_COOKIE, NGM_KSOCKET_SETOPT,
            sockopt, sizeof(sockopt_buf)) == -1) {
        printf( "%s(): Sockopt set failed : %s\n", __FUNCTION__,
                strerror(errno));
        return;
    }
    return;
}
