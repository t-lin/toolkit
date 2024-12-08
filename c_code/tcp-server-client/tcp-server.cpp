/**
 * Largely from Beej's guide.
 */

// C/C++ headers
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/eventfd.h>
#include <sys/epoll.h>

#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include <functional>
#include <atomic>
#include <mutex>

// FUS headers
#include "common.hpp"

#define PORT (3490U) // the port users will be connecting to

#define BACKLOG (10U) // how many pending connections queue will hold

// Handler for SIGCHLD signals
void sigchldHandler([[maybe_unused]] int signal) {
    // waitpid() might overwrite errno, so we save and restore it:
    int savedErrno = errno;

    // Wait for child process to properly be reaped by the system.
    // Yes, we're spin-waiting.
    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;

    errno = savedErrno;
}

class TCPServer {
      private:
    int mListenSocket = -1;
    in_port_t mListenPort = 0;
    std::string mListenAddr;

    std::function<void(int)> mNewConnHandler;

    std::thread mAcceptLoopThread;
    std::atomic_bool mAcceptLoopRunning = false;
    int mShutdownEvFD = -1;

    std::unordered_map<int, std::thread> mClientSock2Thrd;
    std::mutex mMtx;

    [[nodiscard]] bool mCreateServer();
    void mConnHandlerWrapper(const int connectionSocket);
    void mAcceptLoop();

      public:
    TCPServer(const in_port_t portNum);
    ~TCPServer();

    [[nodiscard]] bool runServer(
        const std::function<void(int)> newConnectionHandler);
    void stopServer(const bool disconnectAllClients = true);
    void disconnectClients();
    int listenSocket();
    std::string listenAddress();
};

TCPServer::TCPServer(const in_port_t portNum) : mListenPort(portNum) {
}

TCPServer::~TCPServer() {
    stopServer();
}

bool TCPServer::mCreateServer() {
    if (mListenSocket >= 0) {
        return false;
    }

    struct addrinfo hints; // For now, just a dummy for getaddrinfo()
    memset(&hints, 0, sizeof(hints));

    hints.ai_family = AF_INET;       // For now, restrict to only IPv4
    hints.ai_socktype = SOCK_STREAM; // Use TCP
    hints.ai_flags = AI_PASSIVE;     // Use my IP

    struct addrinfo* pServAddrList = NULL;
    int ret = getaddrinfo(
        NULL, std::to_string(mListenPort).c_str(), &hints, &pServAddrList);
    if (ret != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
        return false;
    }

    // Loop through all the address candidates and bind to the first one we can
    int listenSocket = -1;
    struct addrinfo* pTmpAddr = NULL;
    for (pTmpAddr = pServAddrList; pTmpAddr != NULL;
         pTmpAddr = pTmpAddr->ai_next) {
        // Create socket
        listenSocket = socket(
            pTmpAddr->ai_family, pTmpAddr->ai_socktype, pTmpAddr->ai_protocol);
        if (listenSocket == -1) {
            perror("socket: failed to get file descriptor");
            continue;
        }

        // Set socket options
        int reuseAddrVal = 1; // 1 = "yes"
        if (setsockopt(
                listenSocket, SOL_SOCKET, SO_REUSEADDR, &reuseAddrVal,
                sizeof(reuseAddrVal)) == -1) {
            perror("setsockopt: failed to set socket options");
            return false;
        }

        // Bind socket to the network address
        if (bind(listenSocket, pTmpAddr->ai_addr, pTmpAddr->ai_addrlen) == -1) {
            close(listenSocket);
            perror("bind: failed to bind to socket");
            continue;
        }

        break;
    }

    mListenAddr = getAddrString(pTmpAddr); // First assign for mListenAddr
    std::string fullAddr = mListenAddr + ":" + std::to_string(mListenPort);
    fprintf(stderr, "Listening on %s\n", fullAddr.c_str());

    // Free address candidates list
    freeaddrinfo(pServAddrList);

    if (pTmpAddr == NULL) {
        // We reached the end of the server address list w/o successfully
        // binding to any of them; bail out.
        fprintf(stderr, "server: failed to bind\n");
        return false;
    }

    // Start listening for connections
    if (listen(listenSocket, BACKLOG) == -1) {
        perror("listen");
        return false;
    }

    // First assign for mListenSocket.
    // Do this last, as it's used as an indicator of server readiness.
    mListenSocket = listenSocket;

    return true;
}

void TCPServer::mConnHandlerWrapper(const int connectionSocket) {
    // Wrapper around creation of new connection handler thread.
    // Once the thread ends (i.e. if the handler ends), automatically remove
    // any data/objects associated with this connection/client.
    std::unique_lock<std::mutex> lock(mMtx);

    mClientSock2Thrd[connectionSocket] =
        std::thread(mNewConnHandler, connectionSocket);
    std::thread& handlerThread = mClientSock2Thrd[connectionSocket];
    lock.unlock();

    // Wait for handler to end.
    if (handlerThread.joinable() == true) {
        handlerThread.join();
    }

    // Close socket. The socket may have already closed, so ignore the return
    // on this call if it fails.
    fprintf(stderr, "Wrapper closing socket %d\n", connectionSocket);
    close(connectionSocket);

    // Remove socket & thread.
    lock.lock();
    mClientSock2Thrd.erase(connectionSocket);
    lock.unlock(); // Explicitly call.
}

void TCPServer::mAcceptLoop() {
    // Set up epoll.
    const uint8_t MAX_EVENTS = 5; // Arbitrary; max # of events epoll returns.
    struct epoll_event ev, events[MAX_EVENTS];
    int epollFD = epoll_create1(0);
    if (epollFD < 0) {
        fprintf(stderr, "ERROR: epoll() failed: %s", strerror(errno));
        mAcceptLoopRunning = false;
        return;
    }

    // Add shutdown event FD to the epoll watch list.
    ev.events = EPOLLIN;
    ev.data.fd = mShutdownEvFD;
    if (epoll_ctl(epollFD, EPOLL_CTL_ADD, mShutdownEvFD, &ev) == -1) {
        fprintf(
            stderr, "ERROR: unable to add mShutdownEvFD: %s", strerror(errno));
        mAcceptLoopRunning = false;
        return;
    }

    // Add listening socket to the epoll watch list.
    ev.events = EPOLLIN;
    ev.data.fd = mListenSocket;
    if (epoll_ctl(epollFD, EPOLL_CTL_ADD, mListenSocket, &ev) == -1) {
        fprintf(
            stderr, "ERROR: unable to add mListenSocket: %s", strerror(errno));
        mAcceptLoopRunning = false;
        return;
    }

    // Begin main accept loop.
    int nFDs = 0;
    bool exitLoop = false;
    struct sockaddr_storage clientAddr; // Client's address info
    while (true) {
        // TODO: Should we be masking SIGINT & SIGTERM in this thread?
        //       For now, explicitly check for EINTR and ignore.
        do {
            nFDs = epoll_wait(epollFD, events, MAX_EVENTS, -1);
        } while (nFDs < 0 && EINTR == errno);

        if (nFDs < 0) {
            fprintf(stderr, "ERROR: epoll_wait: %s\n", strerror(errno));
            break;
        }

        // Check if we were woken up by the shutdown FD.
        for (uint8_t i = 0; i < nFDs && exitLoop == false; i++) {
            exitLoop = (events[i].data.fd == mShutdownEvFD);
        }

        if (exitLoop == true) {
            break;
        }

        socklen_t maxSockAddrSz = sizeof(struct sockaddr_storage);
        int connectionSocket = accept(
            mListenSocket, (struct sockaddr*)&clientAddr, &maxSockAddrSz);
        if (connectionSocket == -1) {
            perror("accept: Failed to accept connection");
            continue;
        }

        std::string addrStr = getAddrString(&clientAddr);
        printf(
            "server: got connection from %s:%hu\n", addrStr.c_str(),
            getInetPort((struct sockaddr*)&clientAddr));

        // Acquire lock here, let end of loop implicitly release it.
        std::lock_guard<std::mutex> lock(mMtx);
        if (mClientSock2Thrd.count(connectionSocket) > 0) {
            // The kernel will not re-use sockets unless it's done with.
            // If it does... should we discard the existing thread?
            // For now, do nothing and ignore the new connection.
            fprintf(
                stderr,
                "ERROR: New client's connection socket %d already exists. Not "
                "allocating thread.\n",
                connectionSocket);
            continue;
        }

        // Spawn detatched thread to handle the lifecycle of the connection
        // handler.
        std::thread(&TCPServer::mConnHandlerWrapper, this, connectionSocket)
            .detach();
    }

    mAcceptLoopRunning = false;
    fprintf(stderr, "Exiting accept loop!\n");
    return;
}

bool TCPServer::runServer(const std::function<void(int)> newConnectionHandler) {
    if (mCreateServer() == false) {
        fprintf(
            stderr, "ERROR: Unable to create TCPServer for port %hu\n",
            mListenPort);
        return false;
    } else if (mAcceptLoopRunning == true) {
        fprintf(
            stderr, "ERROR: An instance of the accept loop is already "
                    "running\n");
        return false;
    }

    // Initialize cross-thread signal handling.
    mShutdownEvFD = eventfd(0, 0);
    if (mShutdownEvFD < 1) {
        fprintf(stderr, "ERROR: eventfd() failed: %s", strerror(errno));
        return false;
    }

    // Start new thread for accept loop.
    mNewConnHandler = newConnectionHandler;
    mAcceptLoopRunning = true;
    mAcceptLoopThread = std::thread(&TCPServer::mAcceptLoop, this);

    return true;
}

void TCPServer::stopServer(const bool disconnectAllClients) {
    if (mAcceptLoopRunning == true && mShutdownEvFD >= 0) {
        fprintf( stderr, "Sending signal to shutdown FD\n" );
        uint64_t sig = 1; // Send non-zero signal to shutdown FD.
        ssize_t nBytes = write(mShutdownEvFD, &sig, sizeof(sig));
        if (nBytes != sizeof(sig))
        {
            fprintf( stderr, "ERROR: Unable to write to shutdown FD\n" );
        }
    }

    // Stop accept loop.
    if (mAcceptLoopThread.joinable() == true) {
        mAcceptLoopThread.join();
    }

    // Close listening socket.
    if (mListenSocket >= 0) {
        // Can't do anything if close() fails, so silently ignore the return.
        close(mListenSocket);
    }

    // Optionally, close sockets for existing clients.
    if (disconnectAllClients == true) {
        // In case a newly accepted connection thread is waiting to start,
        // sleep for a second before disconnecting clients.
        sleep(1);
        disconnectClients();
    }
}

void TCPServer::disconnectClients() {
    std::lock_guard<std::mutex> lock(mMtx);
    for (auto& [clientSock, _] : mClientSock2Thrd) {
        if (close(clientSock) != 0) {
            fprintf(
                stderr, "ERROR: Non-graceful close of client socket %d: %s\n",
                clientSock, strerror(errno));
        }
    }

    return;
}

int TCPServer::listenSocket() {
    return mListenSocket;
}

std::string TCPServer::listenAddress() {
    if (mListenSocket < 0) {
        return "";
    }

    return mListenAddr;
}

void newConnectionHandler(int connectionSocket) {
    if (connectionSocket < 0) {
        fprintf(
            stderr, "ERROR: Unable to use connection socket %d\n",
            connectionSocket);
        return;
    }

    uint32_t count = 0;
    char msg[100] = { 0 };
    while (true) {
        // +1 for NULL.
        const int numBytes = sprintf(msg, "Hello, world! %u\n", count++);
        if (numBytes < 0) {
            fprintf(stderr, "ERROR: Unable to write message to buffer\n");
            break;
        }

        // Set MSG_NOSIGNAL so if the other end closes, a Linux signal
        // won't be raised in this process and send() will adequately return.
        if (send(
                connectionSocket, msg, static_cast<size_t>(numBytes + 1),
                MSG_NOSIGNAL) == -1) {
            if (EPIPE == errno || EBADF == errno) {
                fprintf(
                    stderr,
                    "send: Connection associated w/ socket %d has closed\n",
                    connectionSocket);
            } else {
                fprintf(
                    stderr, "send: Failed to send data for socket %d: %s\n",
                    connectionSocket, strerror(errno));
            }

            break;
        }

        sleep(1);
    }

    close(connectionSocket);

    printf("Exiting %s!\n", __FUNCTION__);
    return;
}

int main(void) {
    // We're using threads, not processes; but just in case, catch SIGCHLD.
    // Catch SIGCHLD signals.
    struct sigaction sa;
    sa.sa_handler = sigchldHandler; // Slay all child zombies
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    TCPServer server = TCPServer(static_cast<in_port_t>(PORT));
    if (server.runServer(newConnectionHandler) == false) {
        fprintf(stderr, "ERROR: Unable to start server\n");
        exit(1);
    }

    printf("server: waiting for connections...\n");

    // Block SIGINT & SIGTERM so sigwait() can handle them.
    sigset_t sigSet;
    sigemptyset(&sigSet);
    sigaddset(&sigSet, SIGINT);
    sigaddset(&sigSet, SIGTERM);
    int sig = 0;

    sigprocmask(SIG_SETMASK, &sigSet, NULL);
    sigwait(&sigSet, &sig);

    fprintf(stderr, "Signal caught: %d\n", sig);

    // Explicitly call this (even though will be called on its destruction).
    server.stopServer();

    return 0;
}
