#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <time.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#define MAX_EVENTS  100
#define MAX_BUFFER_SIZE  65535
struct my_tcp_info
{
    uint8_t	tcpi_state;
    uint8_t	tcpi_ca_state;
    uint8_t	tcpi_retransmits;
    uint8_t	tcpi_probes;
    uint8_t	tcpi_backoff;
    uint8_t	tcpi_options;
    uint8_t	tcpi_snd_wscale : 4, tcpi_rcv_wscale : 4;

    uint32_t	tcpi_rto;
    uint32_t	tcpi_ato;
    uint32_t	tcpi_snd_mss;
    uint32_t	tcpi_rcv_mss;

    uint32_t	tcpi_unacked;
    uint32_t	tcpi_sacked;
    uint32_t	tcpi_lost;
    uint32_t	tcpi_retrans;
    uint32_t	tcpi_fackets;

    /* Times. */
    uint32_t	tcpi_last_data_sent;
    uint32_t	tcpi_last_ack_sent;	/* Not remembered, sorry.  */
    uint32_t	tcpi_last_data_recv;
    uint32_t	tcpi_last_ack_recv;

    /* Metrics. */
    uint32_t	tcpi_pmtu;
    uint32_t	tcpi_rcv_ssthresh;
    uint32_t	tcpi_rtt;
    uint32_t	tcpi_rttvar;
    uint32_t	tcpi_snd_ssthresh;
    uint32_t	tcpi_snd_cwnd;
    uint32_t	tcpi_advmss;
    uint32_t	tcpi_reordering;

    uint32_t	tcpi_rcv_rtt;
    uint32_t	tcpi_rcv_space;

    uint32_t	tcpi_total_retrans;
};



struct epoll_event *events = NULL;
int epoll_fd = -1,listen_fd,trigger_count;
char *real_server_name;
unsigned short  real_server_port;
void TimePrinter()
{
    time_t t = time(NULL);
    struct tm* lt = localtime(&t);
    fflush(stdout);
    printf("[ %d-%d-%d %d:%d:%d ] ", lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday, lt->tm_hour, lt->tm_min,
           lt->tm_sec);
}
int setup_socket_server(unsigned short port)
{
    struct sockaddr_in  addr;
    socklen_t addr_length;
    int  fd, opt, opt_length;
    if((fd = socket(AF_INET, SOCK_STREAM|SOCK_NONBLOCK, 0)) < 0)
        return -1;
    opt = 1;
    opt_length = sizeof(opt);
    if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, opt_length) == -1)
        return -1;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    addr.sin_family = AF_INET;
    addr_length = sizeof(addr);
    if(bind(fd, (struct sockaddr *)&addr, addr_length) < 0)
        return -1;
    if(listen(fd, 3) < 0)
        return -1;
    return fd;
}
int setup_epoll(int size, struct epoll_event **events)
{
    int fd;
    if((*events = (struct epoll_event*)malloc(sizeof(struct epoll_event) * size)) == NULL)
        return -1;
    if((fd = epoll_create(size)) == -1)
        free(*events);
    return fd;
}
int add_event(int epoll_fd, int flags, int socket_fd)
{
    struct epoll_event event;
    event.data.fd = socket_fd;
    event.events = flags;
    return epoll_ctl(epoll_fd, EPOLL_CTL_ADD, socket_fd, &event);
}
int delete_event(int epoll_fd, int socket_fd)
{
    return epoll_ctl(epoll_fd, EPOLL_CTL_DEL, socket_fd, NULL);
}
int create_socket(char *host, int port)
{
    int  fd;
    int flag = 1;
    struct sockaddr_in  server_info;
    if((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        return -1;
    server_info.sin_family = AF_INET;
    server_info.sin_port = htons(port);
    server_info.sin_addr.s_addr = inet_addr(host);
    if(connect(fd, (struct sockaddr *)&server_info, sizeof(server_info)) == -1)
        return -1;
    return fd;
}
int is_server_alive(int confd)
{
    struct my_tcp_info info;
    int len=sizeof(info);
    getsockopt(confd, IPPROTO_TCP, TCP_INFO, &info, (socklen_t *)&len);
    if((info.tcpi_state == TCP_ESTABLISHED))
    {
        return 0;
    }
    else{return -1;}
}
void epoll_lt(int sockfd,int sendfd)
{
    char buffer[MAX_BUFFER_SIZE] ;
    int ret;
    memset(buffer, 0, MAX_BUFFER_SIZE);
    ret = recv(sockfd, buffer, MAX_BUFFER_SIZE, 0);
    char s[ret];
    memset(s,0,ret);
    if (ret > 0){
        TimePrinter();
        memcpy(s,buffer,ret-1);
        printf("Receve msg\"%s\"from client.\t  Massage size:%d bytes\n",s, ret-1);
        if(is_server_alive(sendfd) == 0)
        {
            send(sendfd,buffer,MAX_BUFFER_SIZE,0);
        }
    }
    else
    {
        if (ret == 0)
            TimePrinter();
        printf("client close！！！\n");
        close(sockfd);
        delete_event(epoll_fd,sockfd);
    }

}
void epoll_lt1(int sockfd)
{
    int ret;
    char buffer[MAX_BUFFER_SIZE] ;
    memset(buffer,0,MAX_BUFFER_SIZE);
    ret = recv(sockfd,buffer,MAX_BUFFER_SIZE,0);
    char s[ret];
    memset(s,0,ret);
    if (ret > 0) {
        memcpy(s,buffer,ret-1);
        TimePrinter();
        printf("Receve msg\"%s\"from server.\t  Massage size:%d bytes\n", s, ret - 1);
        for(int j = 0;j<trigger_count;j++) {
            if ((events[j].events & EPOLLOUT) && (events[j].data.fd != sockfd)&& (events[j].data.fd != listen_fd)) {
                send(events[j].data.fd, buffer, MAX_BUFFER_SIZE, 0);
            }
        }
    } else {
        if (ret == 0)
            TimePrinter();
        printf("server close！！！\n");
        close(sockfd);
    }
}

int main(int argc, char *argv[])
{
    struct sockaddr_in  addr;
    socklen_t addr_length;
    int  i, client_fd,user_fd;
    unsigned short  port;
    if(argc != 6)
    {
        fprintf(stdout, "usage: pfwd -l [listen port] -c [forward host] [forward port]\n");
        fprintf(stdout, "\tlisten port:  The port pfwd sould listen on.\n");
        fprintf(stdout, "\tforward host: The forwarding server ip.\n");
        fprintf(stdout, "\tforward port: The forwarding server port.\n\n");
        exit(1);
    }
    for( i = 1;i<argc;) {
        if(!strcmp(argv[i],"-c"))
        {
            real_server_name = argv[i+1];
            real_server_port = (unsigned short)atoi(argv[i+2]);
            i = i+3;
        }
        else if(!strcmp(argv[i],"-l"))
        {
            port = (unsigned short)atoi(argv[i + 1]);
            i =i+2;
        }
    }
    if((listen_fd = setup_socket_server(port)) == -1)
        fprintf(stderr,"Unable to setup socket.");
    if((epoll_fd = setup_epoll(MAX_EVENTS, &events)) == -1)
        fprintf(stderr,"Unable to setup Epoll events.");
    if(add_event(epoll_fd, EPOLLIN, listen_fd) == -1)
        fprintf(stderr,"Unable to add listening socket to Epoll event queue.");
    fprintf(stdout, "Resquests in port %d will be forwarded to %s:%d...\n", port, real_server_name, real_server_port);
    addr_length = sizeof(addr);
    user_fd =-1;
    while(1)
    {
        if(is_server_alive(user_fd) == -1)
        {
            close(user_fd);
            user_fd = create_socket(real_server_name, real_server_port);
            add_event(epoll_fd,EPOLLIN,user_fd);
        }
        if((trigger_count = epoll_wait(epoll_fd, events, MAX_EVENTS, -1)) < 0)
            fprintf(stderr,"Unable to Epoll wait.");
        for(i = 0; i < trigger_count; i++)
        {
            client_fd = events[i].data.fd;
            if(events[i].events & (EPOLLHUP | EPOLLERR))
            {
                fprintf(stderr, "Client disconnected (socket %d).\n", client_fd);
                delete_event(epoll_fd, client_fd);
                break;
            }
            else if(client_fd == listen_fd)
            {
                if((client_fd= accept(listen_fd, (struct sockaddr *)&addr, &addr_length)) == -1)
                {
                    fprintf(stderr, "Unable to accept new client connection.\n");
                    continue;
                }
                if(add_event(epoll_fd, EPOLLIN|EPOLLOUT, client_fd) == -1)
                {
                    fprintf(stderr, "Unable to add soecket to Epoll event queue, closing (socket %d).\n", client_fd);
                    close(client_fd);
                    continue;
                }
                TimePrinter();
                fprintf(stdout, "New client connection from %s (socket %d).\n", inet_ntoa(addr.sin_addr), client_fd);
            }
            else if ((events[i].events & EPOLLIN))
            {
                if(is_server_alive(user_fd) == 0){
                    if (client_fd == user_fd)
                    {
                        epoll_lt1(client_fd);
                    }
                    else{
                        epoll_lt(client_fd,user_fd);
                    }
                }
            }
        }
    }
    exit(0);
}