#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <linux/if.h> // This should be BEFORE net/if.h
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <features.h>
#include <sys/ioctl.h>
//#include <net/if.h>
#include <netinet/in.h>
#include <signal.h>

unsigned char my_mac[10] = {0}; // Filled later in main()
unsigned char final_mac[10] = {0};

#define LISTEN_PROTOCOL   ETH_P_IP
//#define LISTEN_PROTOCOL   0x0801
#define INSPECT_UDP_PORT  5001

#define MAX_SEARCHWORD_LENGTH 20

typedef struct OffsetMatch {
    int offset;
    char matchStr[MAX_SEARCHWORD_LENGTH + 1];
    int wordLength;
} OffsetMatch;

static volatile bool keepRunning = true;
void interruptHandler(const int signal) {
    keepRunning = false;
}

int BindRawSocketToInterface(const char *device, int rawsock, int protocol)
{
    struct sockaddr_ll packet_info;
    struct ifreq ifr;

    /* First Get the Interface Index  */
    strncpy((char *)ifr.ifr_name, device, IFNAMSIZ);
    if((ioctl(rawsock, SIOCGIFINDEX, &ifr)) == -1)
    {
        printf("Error getting Interface index !\n");
        exit(-1);
    }



    /* Bind our raw socket to this interface */

    packet_info.sll_family = AF_PACKET;
    packet_info.sll_ifindex = ifr.ifr_ifindex;
    packet_info.sll_protocol = htons(protocol);


    if((bind(rawsock, (struct sockaddr *)&packet_info, sizeof(packet_info)))== -1)
    {
        perror("Error binding raw socket to interface\n");
        exit(-1);
    }

    return 1;

}

void sendthepacket(const char *device, int rawsock, int protocol, char *Frame_TX, int FrameSize_TX)
{
    static int send_result;
    static struct sockaddr_ll sll;
    static struct ifreq ifr;
    const static unsigned int SSL_SIZE = sizeof(struct sockaddr_ll);
    static bool initInterface = false;

    if ( !initInterface ) {
        // Use bzero here as it's slightly less lines of assembly code than memset
        bzero(&sll, sizeof(sll));
        bzero(&ifr, sizeof(ifr));

        /* First Get the Interface Index  */
        strncpy((char *)ifr.ifr_name, device, IFNAMSIZ);
        if((ioctl(rawsock, SIOCGIFINDEX, &ifr)) == -1)
        {
            printf("Error getting Interface index !\n");
            exit(-1);
        }

        /* Bind our raw socket to this interface */

        sll.sll_family = AF_PACKET;
        sll.sll_ifindex = ifr.ifr_ifindex;
        sll.sll_protocol = htons(protocol);
        //printf("\n The sending device is: %s\n", device );
    }

    send_result = sendto(rawsock, Frame_TX, FrameSize_TX, 0,(struct sockaddr*)&sll, SSL_SIZE );//len+14 //ETH_DATA_LEN1+14

    if ( (send_result == -1) || ( send_result != FrameSize_TX) ){
        perror( " Sending: " );
        printf("\n  TRANSMIT ERROR 2 \n");

    }
    else {
        // All good
        //printf("packet sent to %s\n", device);
        ;
    }

    return;
}


// iface = String w/ interface name
// macAddr = Array to store resulting binary representation of address
bool getMACAddress(const char *iface, unsigned char *macAddr) {
    // WARNING: For the moment, this code only works in Linux
    char ifaceAddrPath[50] = "/sys/class/net/";
    char ifaceMAC[18] = {0}; // 12 characters + 5 colons + NULL
    FILE *pFile = NULL;

    // Basically: ifaceAddrPath = "/sys/class/net/" + iface + "/address"
    strncpy(ifaceAddrPath + strlen(ifaceAddrPath), iface, strlen(iface) + 1);
    strncpy(ifaceAddrPath + strlen(ifaceAddrPath), "/address", strlen("/address") + 1);

    pFile = fopen(ifaceAddrPath, "r");
    if (pFile != NULL) {
        fscanf(pFile, "%s", ifaceMAC);
        fclose(pFile);
        //printf("ifaceMAC is: %s\n", ifaceMAC);

        if( 6 != sscanf(ifaceMAC, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                        &macAddr[0], &macAddr[1], &macAddr[2],
                        &macAddr[3], &macAddr[4], &macAddr[5] ) ) {
            printf("ERROR: Conversion of %s's MAC to binary failed\n", iface);
            return false;
        }
    }
    else {
        return false;
    }

    return true;
}

bool parseMatches(OffsetMatch *matchArray, const char **matchStrings, int nSignatures) {
    char *pComma = NULL;
    for (int i = 0; i < nSignatures; i++) {
        pComma = strchr(matchStrings[i], ',');

        if (pComma != NULL && pComma != matchStrings[i]) {
            matchArray[i].offset = atoi(matchStrings[i]); // atoi() terminates on non-digit
            strncpy(matchArray[i].matchStr, pComma + 1, MAX_SEARCHWORD_LENGTH);
            matchArray[i].wordLength = strlen(pComma + 1);//, MAX_SEARCHWORD_LENGTH);
        }
        else {
            //printf("ERROR: Expecting match format to be composed of an offset and a string separated by a comma\n");
            //printf("\te.g. 88,hello 155,WORLD");

            return false;
        }
    }

    return true;
}

int main(int argc, const char **argv) {
    if (argc <= 3) {
        printf("ERROR: Expecting at least 3 parameters:\n"
                "\t- (Mandatory) Interface name for listening\n"
                "\t- (Mandatory) Interface name for forwarding/sending\n"
                "\t- (Mandatory) Destination MAC address\n"
                "\t- (Optional) One or more offset,strings (space-separated) to match for\n");
        printf("\n\te.g. ./intercept eth0 eth1 de:ad:be:ef:12:34 88,hello 155,WORLD\n");
        return 1;
    }

    signal(SIGINT, interruptHandler);

    const char *listenIfaceName = argv[1];
    const char *forwardIfaceName = argv[2];
    const char *destMAC = argv[3];

    const char **matchStrings = NULL;
    int nSignatures = 0;
    OffsetMatch *matchSigs = NULL;
    if (argc > 3) { // Match signatures exist
        matchStrings = argv + 4;
        nSignatures = argc - 4;

        //for (int idx = 0; idx < nSignatures; idx++)
        //    printf("String %d is: %s\n", idx, matchStrings[idx]);

        matchSigs = malloc(nSignatures * sizeof(OffsetMatch));
        if (matchSigs != NULL) {
            memset(matchSigs, 0x0, nSignatures * sizeof(OffsetMatch));
        }
        else {
            printf("ERROR: Malloc failure for matchSigs");
            return 1;
        }

        // Store arguments into structs
        if ( !parseMatches(matchSigs, matchStrings, nSignatures) ) {
            printf("ERROR: Expecting match format to be composed of an offset and a string separated by a comma\n");
            printf("\te.g. 88,hello 155,WORLD\n");
            return 1;
        }
    }

    //for (int idx = 0; idx < nSignatures; idx++)
    //    printf("Offset %d with string %s\n", matchSigs[idx].offset, matchSigs[idx].matchStr);

    if ( !getMACAddress(forwardIfaceName, my_mac) ) {
        printf("ERROR: Failed to get MAC address\n");
        return 1;
    }
    else if (6 != sscanf(destMAC, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                        &final_mac[0], &final_mac[1], &final_mac[2],
                        &final_mac[3], &final_mac[4], &final_mac[5] ) ) {
        printf("ERROR: Conversion of destination MAC to binary failed\n");
    }

    // Start listening and capturing packets...
    unsigned int sock = 0, nRecvBytes = 0;
    unsigned long long count = 0;
    char buffer[2048];

    struct ethhdr *ethHead = NULL;
    struct iphdr *ipHead = NULL;
    struct udphdr *udpHead = NULL;
    char *data = NULL;

    if ( (sock = socket(PF_PACKET, SOCK_RAW, htons(LISTEN_PROTOCOL)))<0) { // ETH_P_IP is just an example
        perror("socket");
        exit(1);
    }
    BindRawSocketToInterface(listenIfaceName, sock, LISTEN_PROTOCOL);

    // We expect IP headers to have no options
    const unsigned int IP_HLEN = sizeof(struct iphdr);
    const unsigned int UDP_HLEN = sizeof(struct udphdr);
    const unsigned int DATA_OFFSET = ETH_HLEN + IP_HLEN + UDP_HLEN;

    unsigned int sigIdx = 0; // Iterator for signatures

    while (keepRunning) {
        nRecvBytes = recvfrom(sock, buffer, 2048, 0, NULL, NULL);
        buffer[nRecvBytes] = 0; // NULL-terminate for string.h functions

        /* Check to see if the packet contains at least
        * complete Ethernet (14), IP (20) and TCP/UDP
        * ports (8) headers.
        */
        if (nRecvBytes < DATA_OFFSET) {
            perror("recvfrom():");
            printf("Incomplete packet (errno is %d)\n", errno);
            close(sock);
            exit(1);
        }

        ethHead = (struct ethhdr *)buffer;

        ipHead = (struct iphdr *)(buffer + ETH_HLEN); /* Skip Ethernet header */

        if (ipHead->version == 0x4 && ipHead->ihl == 0x5) { /* Double check for IPv4
                                                             * and no options present */
            if (ipHead->protocol == IPPROTO_UDP)
            {
                udpHead = (struct udphdr *)((char *)ipHead + IP_HLEN); // No options, since IHL = 5
                if (htons(udpHead->dest) == INSPECT_UDP_PORT) {
                    count++;
                    if (count % 1000000 == 0)
                        printf("%llu\n", count);
                    //memcpy(ethHead->h_dest, (char *)final_mac, 6);
                    //memcpy(ethHead->h_source, (char *)my_mac, 6);

                    // Zero out the checksum (if changing UDP header or payload, this should be done)
                    //udpHead->check = 0;

                    data = (char *)udpHead + UDP_HLEN;

                    // Iterate through search words, for each word go through data payload
                    for (sigIdx = 0; sigIdx < nSignatures; sigIdx++) {
                        // Calculate how many 8-byte boundary areas we need to search
                        //unsigned int SEARCH_BOUNDARY = 8;
                        //unsigned int nSearchables = (nRecvBytes - DATA_OFFSET) / SEARCH_BOUNDARY;
                        //const char *word = matchStrings[sigIdx];
                        //unsigned int wordLength = strlen(word);
                        static int searchRes = 0;

                        searchRes = strncmp(data + matchSigs[sigIdx].offset,
                                            matchSigs[sigIdx].matchStr,
                                            matchSigs[sigIdx].wordLength);
                        if (searchRes == 0) {
                            //count++;
                            printf("Found word: %s, total search words found %llu\n",
                                        matchSigs[sigIdx].matchStr, count);
                        }
                    }


                    //printf("payload changed to %s\n", data);
                    sendthepacket(forwardIfaceName, sock, LISTEN_PROTOCOL, buffer, nRecvBytes);
                }
            }
        } else {
            // Do nothing?
            continue;
        }
    }

    if (matchSigs) {
        free(matchSigs);
        matchSigs = NULL;
    }

    return 0;
}

