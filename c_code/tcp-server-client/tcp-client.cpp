/**
 * Largely from Beej's guide.
 */

// C/C++ headers
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <signal.h>

// FUS headers
#include "common.hpp"

#define PORT ("3490")      // Server port
#define MAXDATASIZE (100U) // max number of bytes we can get at once

static bool gINTERRUPTED = false;
static void signalHandler(int sigVal) {
    fprintf(stderr, "\n\nCaught signal %d\n\n", sigVal);
    gINTERRUPTED = true;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "usage: client hostname\n");
        exit(1);
    }

    // Setup signal handler
    struct sigaction action;
    action.sa_handler = signalHandler;
    action.sa_flags = 0;
    sigemptyset(&action.sa_mask);
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);

    struct addrinfo hints; // For now, just a dummy for getaddrinfo()
    memset(&hints, 0, sizeof(hints));

    hints.ai_family = AF_INET;       // For now, restrict to only IPv4
    hints.ai_socktype = SOCK_STREAM; // Use TCP

    // Resolve server's address
    struct addrinfo* pServAddrList = NULL;
    int ret = getaddrinfo(argv[1], PORT, &hints, &pServAddrList);
    if (ret != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
        exit(1);
    }

    // Loop through all the address candidates and connect to the first one we
    // can
    int connSocket = 0;
    struct addrinfo* pTmpAddr = NULL;
    for (pTmpAddr = pServAddrList; pTmpAddr != NULL;
         pTmpAddr = pTmpAddr->ai_next) {
        // Create socket
        connSocket = socket(
            pTmpAddr->ai_family, pTmpAddr->ai_socktype, pTmpAddr->ai_protocol);
        if (connSocket == -1) {
            perror("client: socket");
            continue;
        }

        // Connect to the server's network address
        if (connect(connSocket, pTmpAddr->ai_addr,
                    pTmpAddr->ai_addrlen) == -1) {
            perror("client: connect");
            close(connSocket);
            continue;
        }

        break;
    }

    if (pTmpAddr == NULL) {
        // We reached the end of the server address list w/o successfully
        // connecting to any of them; bail out.
        fprintf(stderr, "client: failed to connect\n");
        exit(2);
    }

    std::string addrStr = getAddrString(pTmpAddr);
    printf("client: connecting to %s:%s\n", addrStr.c_str(), PORT);

    // Free address candidates list
    freeaddrinfo(pServAddrList);

    char buf[MAXDATASIZE] = { 0 };
    ssize_t numBytes = -1;
    while (gINTERRUPTED == false) {
        numBytes = recv(connSocket, buf, (size_t)(MAXDATASIZE - 1), 0);
        if (0 == numBytes) {
            fprintf(
                stderr, "Connection associated with socket %d has closed\n",
                connSocket);
            break;
        } else if (-1 == numBytes) {
            fprintf(stderr, "ERROR: recv: %s\n", strerror(errno));
            break;
        }

        buf[numBytes] = '\0';

        printf("client received: %s\n", buf);

        sleep(1);
    }

    close(connSocket);

    printf("Bye!\n");

    return 0;
}
