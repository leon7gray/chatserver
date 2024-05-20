#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "debug.h"
#include "server.h"
#include "globals.h"
#include "csapp.h"
#include "protocol.h"

volatile sig_atomic_t sighup_flag = 0;
pthread_t threads[MAX_CLIENTS];
int numThreads = 0;

static void terminate(int);

void sighup_handler(int signum)
{
    sighup_flag = 1;
}

/*
 * "Charla" chat server.
 *
 * Usage: charla <port>
 */
int main(int argc, char* argv[])
{
    if (argc != 3)
    {
        printf("Usage: bin/charla -p <port>\n");
        exit(0);
    }
    //int* fds[MAX_CLIENTS];
    // Option processing should be performed here.
    // Option '-p <port>' is required in order to specify the port number
    // on which the server should listen.
    char* port;

    if(strcmp(argv[1], "-p") == 0)
    {
        port = argv[2];
    }
    else
    {
        printf("Usage: bin/charla -p <port>\n");
        exit(0);
    }
    // Perform required initializations of the client_registry and
    // player_registry.
    user_registry = ureg_init();
    client_registry = creg_init();

    // TODO: Set up the server socket and enter a loop to accept connections
    // on this socket.  For each connection, a thread should be started to
    // run function charla_client_service().  In addition, you should install
    // a SIGHUP handler, so that receipt of SIGHUP will perform a clean
    // shutdown of the server.

    struct sigaction action;
    action.sa_handler = sighup_handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    sigaction(SIGHUP, &action, NULL);

    int listenfd, *connfdp;
    struct sockaddr_storage clientaddr;
    socklen_t clientlen = sizeof(clientaddr);
    listenfd = Open_listenfd(port);

    while (1)
    {
        if (sighup_flag == 1)
        {
            break;
        }
        connfdp = malloc(sizeof(int));
        //printf("%p\n", connfdp);
        *connfdp = accept(listenfd, (SA *) &clientaddr, &clientlen);
        if (*connfdp < 0)
        {
            if (errno == EINTR)
            {
                free(connfdp);
                break;
            }
        }
        //fds[numThreads] = connfdp;
        pthread_create(&threads[numThreads], NULL, chla_client_service, connfdp);
        numThreads++;
    }
    /*
    for (int i = 0; i < numThreads; i++)
    {
        //printf("%s %p\n", "THREAD FD: ", fds[i]);
        //printf("%s %d\n", "THREAD FD: ", *fds[i]);
        printf("%lu\n", threads[i]);
        //close(*fds[i]);
        //free(fds[i]);
        pthread_detach(threads[i]);
        printf("%s\n", "FREED");
    }
    */
    terminate(0);
}

/*
 * Function called to cleanly shut down the server.
 */
static void terminate(int status) {
    // Shut down all existing client connections.
    // This will trigger the eventual termination of service threads.
    creg_shutdown_all(client_registry);
    //printf("SHUTDOWN ALL COMPLETED\n");
    // Finalize modules.
    creg_fini(client_registry);
    ureg_fini(user_registry);
    for (int i = 0; i < numThreads; i++)
    {
        //printf("%lu\n", threads[i]);
        pthread_join(threads[i], NULL);
        //printf("%s\n", "FREED");
    }
    //printf("ALL FINALIZED\n");
    //debug("%ld: Server terminating", pthread_self());
    exit(status);
}
