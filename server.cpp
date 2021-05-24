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

#define PORT "3490"

#define BACKLOG 10

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
    int sockfd, new_fd;

//    struct addrinfo {
//        int              ai_flags;     // AI_PASSIVE, AI_CANONNAME, etc.
//        int              ai_family;    // AF_INET, AF_INET6, AF_UNSPEC
//        int              ai_socktype;  // SOCK_STREAM, SOCK_DGRAM
//        int              ai_protocol;  // use 0 for "any"
//        size_t           ai_addrlen;   // size of ai_addr in bytes
//        struct sockaddr *ai_addr;      // struct sockaddr_in or _in6
//        char            *ai_canonname; // full canonical hostname
//
//        struct addrinfo *ai_next;      // linked list, next node
//    };

//    struct sockaddr {
//        unsigned short    sa_family;    // address family, AF_xxx
//        char              sa_data[14];  // 14 bytes of protocol address
//    };

// (IPv4 only--see struct sockaddr_in6 for IPv6)

//    struct sockaddr_in {
//        short int          sin_family;  // Address family, AF_INET
//        unsigned short int sin_port;    // Port number
//        struct in_addr     sin_addr;    // Internet address
//        unsigned char      sin_zero[8]; // Same size as struct sockaddr
//    };

// (IPv4 only--see struct in6_addr for IPv6)

    // Internet address (a structure for historical reasons)
//    struct in_addr {
//        uint32_t s_addr; // that's a 32-bit int (4 bytes)
//    };

// (IPv6 only--see struct sockaddr_in and struct in_addr for IPv4)

//    struct sockaddr_in6 {
//        u_int16_t       sin6_family;   // address family, AF_INET6
//        u_int16_t       sin6_port;     // port number, Network Byte Order
//        u_int32_t       sin6_flowinfo; // IPv6 flow information
//        struct in6_addr sin6_addr;     // IPv6 address
//        u_int32_t       sin6_scope_id; // Scope ID
//    };
//
//    struct in6_addr {
//        unsigned char   s6_addr[16];   // IPv6 address
//    };

//    struct sockaddr_storage {
//        sa_family_t  ss_family;     // address family
//
//        // all this is padding, implementation specific, ignore it:
//        char      __ss_pad1[_SS_PAD1SIZE];
//        int64_t   __ss_align;
//        char      __ss_pad2[_SS_PAD2SIZE];
//    };

//    struct sockaddr_in sa; // IPv4
//    struct sockaddr_in6 sa6; // IPv6
//
//    inet_pton(AF_INET, "10.12.110.57", &(sa.sin_addr)); // IPv4
//    inet_pton(AF_INET6, "2001:db8:63b3:1::3490", &(sa6.sin6_addr)); // IPv6

//    char ip4[INET_ADDRSTRLEN];  // space to hold the IPv4 string
//    struct sockaddr_in sa;      // pretend this is loaded with something
//
//    inet_ntop(AF_INET, &(sa.sin_addr), ip4, INET_ADDRSTRLEN);
//
//    printf("The IPv4 address is: %s\n", ip4);
//
//
//// IPv6:
//
//    char ip6[INET6_ADDRSTRLEN]; // space to hold the IPv6 string
//    struct sockaddr_in6 sa6;    // pretend this is loaded with something
//
//    inet_ntop(AF_INET6, &(sa6.sin6_addr), ip6, INET6_ADDRSTRLEN);
//
//    printf("The address is: %s\n", ip6);


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

        printf("The IPv4 address is: %s\n", ip4);

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
            if (send(new_fd, "Hello, world!", 13, 0) == -1)
                perror("send");
            close(new_fd);
            exit(0);
        }

        close(new_fd); // parent doesn't need thi
    }

    return 0;
}




