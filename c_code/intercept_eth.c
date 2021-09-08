#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
//#include <linux/in.h>
#include <linux/if_ether.h>
#include <features.h>
#include <linux/if_packet.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>

char my_mac[100] = {0xfa, 0x16, 0x3e, 0x8d, 0x10, 0xf6}; // 8d:10:f6
char final_mac[100] = {0xfa, 0x16, 0x3e, 0x0e, 0xc5, 0x9b}; // fpgademo 10.2.5.21
//char final_mac[100] = {0xfa, 0x16, 0x3e, 0xac, 0x99, 0x9f};

#define LISTEN_PROTOCOL   ETH_P_IP
//#define LISTEN_PROTOCOL   0x0801
#define INSPECT_UDP_PORT  5001

#define INCOMING_FACE "eth0"
#define OUTGOING_IFACE "eth0"

int BindRawSocketToInterface(char *device, int rawsock, int protocol)
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

int sendthepacket(char *device, int rawsock, int protocol, char *Frame_TX, int FrameSize_TX)
{
    int send_result;
    struct sockaddr_ll sll;
    struct ifreq ifr;

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

    send_result = sendto(rawsock, Frame_TX, FrameSize_TX, 0,(struct sockaddr*)&sll, sizeof(sll) );//len+14 //ETH_DATA_LEN1+14

    if ( (send_result == -1) || ( send_result != FrameSize_TX) ){
        perror( " Sending: " );
        printf("\n  TRANSMIT ERROR 2 \n");

    }
    else {
        //printf("packet sent to %s\n", device);
        ;
    }

}

int main(int argc, char **argv) {
    unsigned int sock = 0, n = 0, i = 0, cnt = 0;
    char buffer[2048];

    char src_ip[10];
    char dst_ip[10];
    char src_prt[10];
    char dst_prt[10];
    char tmp;

    unsigned char *iphead, *ethhead, *data;
    unsigned short payload_len;

    if ( (sock=socket(PF_PACKET, SOCK_RAW, htons(LISTEN_PROTOCOL)))<0) { // ETH_P_IP is just an example
        perror("socket");
        exit(1);
    }
    BindRawSocketToInterface(INCOMING_FACE, sock, LISTEN_PROTOCOL);

    while (1) {
        //cnt++;
        //printf("----------\n");
        n = recvfrom(sock, buffer, 2048, 0, NULL, NULL);
        //printf("%02x:", buffer[5]);
        //    printf("----------\n");
        //   printf("msg %d:%d bytes read\n",cnt,n);

        /* Check to see if the packet contains at least
        * complete Ethernet (14), IP (20) and TCP/UDP
        * (8) headers.
        */
        if (n<42) {
            perror("recvfrom():");
            printf("Incomplete packet (errno is %d)\n", errno);
            close(sock);
            exit(0);
        }

        ethhead = buffer;

        if ( !strncmp(ethhead, my_mac, 6) ) {
            // Destination == my_mac, now check if UDP port is 5001 (default iperf port)

            iphead = buffer + 14; /* Skip Ethernet header */
            if (*iphead==0x45) { /* Double check for IPv4
                                  * and no options present */

                // Check if L4 protocol is UDP and destination port matches
                if (iphead[9] == 17 &&
                    ((iphead[22]<<8)+iphead[23]) == INSPECT_UDP_PORT) {

                    // Change destination MAC to final_mac
                    strncpy(ethhead, final_mac, 6);

                    // Change source MAC to my_mac
                    strncpy(ethhead+6, my_mac, 6);

                    payload_len = (iphead[24] << 8) + iphead[25] - 8;
                    //printf("length = %u\n", payload_len);
                    data = iphead + 28;

                    for (i = 0; i < payload_len; i++) {
                        data[i] = ~data[i]; // Flip bits

                        // Left-shift every byte
                        if (i == 0)
                            tmp = data[0];
                        else if (i == payload_len - 1)
                            data[i] = tmp;
                        else
                            data[i - 1] = data[i];
                    }

                    // Zero out the checksum to indicate it's unused
                    iphead[26] = iphead[27] = 0;

                    sendthepacket(OUTGOING_IFACE, sock, LISTEN_PROTOCOL, buffer, n);
                }
            }
        }
        else {
            // Destination does not match my_mac
            continue;
        }

        //printf("Destination MAC address: %02x:%02x:%02x:%02x:%02x:%02x\n",
        //                ethhead[0],ethhead[1],ethhead[2],
        //                ethhead[3],ethhead[4],ethhead[5]);
        //printf("Source MAC address: %02x:%02x:%02x:%02x:%02x:%02x\n",
        //                ethhead[6],ethhead[7],ethhead[8],
        //                ethhead[9],ethhead[10],ethhead[11]);
        //printf("EtherType: %02x%02x\n", ethhead[12], ethhead[13]);

        //iphead = buffer+14; /* Skip Ethernet header */

    }

    return 0;
}

