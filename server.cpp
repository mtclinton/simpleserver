#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <iostream>
#include <string>
#include "getip.hpp"

#define PORT "3490"

#define BACKLOG 10

#define MAXDATASIZE 100 // max number of bytes we can get at once




void sigchld_handler(int s) {
    int saved_errno = errno;

    while(waitpid(-1, NULL, WNOHANG) >0);

    errno = saved_errno;
}

void *get_in_addr(struct sockaddr *sa){
    if(sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(void) {
    int sockfd, new_fd, numbytes;
    char buf[MAXDATASIZE];

    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr;
    socklen_t sin_size;
    struct sigaction sa;
    int yes = 1;
    char s[INET6_ADDRSTRLEN];
    int rv;


    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if((rv = getaddrinfo("0.0.0.0", PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    char hostname[1024];
    hostname[1023] = '\0';
    gethostname(hostname, 1023);
    printf("Hostname: %s\n", hostname);

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                       sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        char ip4[INET_ADDRSTRLEN];

        inet_ntop(AF_INET, &(p->ai_addr), ip4, INET_ADDRSTRLEN);

        printf("The my address is: %s\n", ip4);

//        for (struct addrinfo *ai = p; ai != NULL; ai = ai->ai_next) {
//            printf("address: %s -> %s\n", ai->ai_canonname,
//                   inet_ntop(ai->ai_family, ai->ai_addr, s, sizeof(s)));
//        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo); // all done with this structure

    if (p == NULL) {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    printf("server: waiting for connections...\n");

    while(1) { // main accept() loop
        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *) &their_addr, &sin_size);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }

        inet_ntop(their_addr.ss_family,
                  get_in_addr((struct sockaddr *) &their_addr),
                  s, sizeof s);
        printf("server: got connection from %s\n", s);



        if (!fork()) { // this is the child process
            close(sockfd); // child doesn't need the listener

            if ((numbytes = recv(new_fd, buf, MAXDATASIZE-1, 0)) == -1) {
                perror("recv");
                exit(1);
            }

            buf[numbytes] = '\0';

            printf("server: received '%s'\n",buf);

            char *domain=buf;
            char domain_ip[INET_ADDRSTRLEN];

            getip(domain, domain_ip);

            std::cout << "The domain ip is: "  << domain_ip << std::endl;

            if (send(new_fd, &domain_ip, strlen(domain_ip), 0) == -1)
                perror("send");
            close(new_fd);
            exit(0);
        }

        close(new_fd); // parent doesn't need thi
    }

    return 0;
}




