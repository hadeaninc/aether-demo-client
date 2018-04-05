/*
   Copyright 2018 Hadean Supercomputing Inc.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#pragma once

#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include <timer.hh>

static void recv_all(int sockfd, void* data, size_t size) {
    size_t recvbytes;
    int n;
    for (recvbytes = 0; recvbytes < size; recvbytes += n) {
        n = recv(sockfd, (char*)data+recvbytes, size-recvbytes, MSG_WAITALL);
        if (n == -1) {
            perror("recv");
            exit(1);
        }
    }
    assert(recvbytes == size);
}

static void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &((struct sockaddr_in*)sa)->sin_addr;
    }
    return &((struct sockaddr_in6*)sa)->sin6_addr;
}

static int connect_to_host_port(const char* host, const char* port) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo *servinfo;
    int rv = getaddrinfo(host, port, &hints, &servinfo);
    if (rv != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return -1;
    }
    struct addrinfo *p;
    int sockfd;
    for (p = servinfo; p != NULL; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1) {
            perror("socket");
            continue;
        }
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            perror("connect");
            close(sockfd);
            continue;
        }
        break;
    }
    if (!p) {
        fprintf(stderr, "failed to connect\n");
        return -1;
    }
    char host_str[INET6_ADDRSTRLEN];
    inet_ntop(p->ai_family, get_in_addr((struct sockaddr*)p->ai_addr), host_str, sizeof(host_str));
    int one = 1;
    setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    freeaddrinfo(servinfo);
    return sockfd;
}

static int connect_to_host_port_with_timeout(const char* host, const char* port) {
    struct timespec end = timer_add(timer_get(), 10 * 1000000000ULL);
    while (true) {
        if (timer_diff(timer_get(), end) < 0) {
            int r = connect_to_host_port(host, port);
            if (r >= 0)
                return r;
        } else {
            fprintf(stderr, "timed out connecting to %s:%s\n", host, port);
            return -1;
        }
        sleep(1);
    }
}

static void close_socket(const int sockfd) {
    close(sockfd);
}
