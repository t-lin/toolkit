#include <stdio.h>
#include <signal.h>
#include <unistd.h>

static bool gINTERRUPTED = false; // Has program caught any OS signals

static void signalHandler(int sigVal) {
    printf("\n\nSignal caught!\n\n");
    gINTERRUPTED = true;
}

int main(int argc, char* const* argv)
{
    // Set up signal catching
    struct sigaction action;
    action.sa_handler = signalHandler;
    action.sa_flags = 0;
    sigemptyset (&action.sa_mask);
    sigaction (SIGINT, &action, NULL);
    sigaction (SIGTERM, &action, NULL);

    unsigned long long count = 0;
    while (true) {
        // Some random loop
        printf("Loop count: %llu\n", ++count);
        sleep(1);

        if (gINTERRUPTED) {
            printf("Breaking from loop...\n");
            break;
        }
    }

    return 0;
}
