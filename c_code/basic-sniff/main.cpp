#include <iostream>
#include <iomanip>
#include <unordered_map>
#include <signal.h>
#include <sys/time.h>

// Packet processing libs
#include <ifaddrs.h>
#include <netinet/in.h>
#include <tins/tins.h>

//using namespace std;
using std::cout;
using std::endl;
using std::string;
using std::unordered_map;
using namespace Tins;

#define CAP_LEN 140
#define THOUSAND 1000
#define MILLION 1000000

static Sniffer *sniffer = nullptr;

static void signalHandler(int sigVal) {
    if (sniffer)
        sniffer->stop_sniff();
}

// From pping (https://github.com/pollere/pping)
// return the local ip address of 'ifname'
// XXX since an interface can have multiple addresses, both IP4 and IP6,
// this should really create a set of all of them and later test for
// membership. But for now we just take the first IP4 address.
static std::string localAddrOf(const std::string ifname)
{
    std::string local{};
    struct ifaddrs* ifap;

    if (getifaddrs(&ifap) == 0) {
        for (auto ifp = ifap; ifp; ifp = ifp->ifa_next) {
            if (ifname == ifp->ifa_name &&
                  ifp->ifa_addr->sa_family == AF_INET) {
                uint32_t ip = ((struct sockaddr_in*)
                               ifp->ifa_addr)->sin_addr.s_addr;
                local = IPv4Address(ip).to_string();
                break;
            }
        }
        freeifaddrs(ifap);
    }
    return local;
}

int main(int argc, char *argv[]) {
    // Set up signal catching
    struct sigaction action;
    action.sa_handler = signalHandler;
    action.sa_flags = 0;
    sigemptyset (&action.sa_mask);
    sigaction (SIGINT, &action, NULL);
    sigaction (SIGTERM, &action, NULL);

    string iface;
    string filter;

    if (argc == 1) {
        cout << "Usage: ./blek <interface name> \"<optional filter (pcap format)>\"" << endl;
        exit(0);
    } else if (argc == 2) {
        iface = argv[1];
        if (localAddrOf(iface).empty()) {
            cout << "ERROR: Could not identify interface" << endl;
            exit(1);
        }
    } else {
        iface = argv[1];
        filter = argv[2];
    }

    //Sniffer *sniffer = nullptr;
    SnifferConfiguration config;
    if (!filter.empty())
        config.set_filter(filter);
    config.set_promisc_mode(false);
    config.set_snap_len(CAP_LEN);
    //config.set_timeout(3000);
    config.set_immediate_mode(true);

    sniffer = new Sniffer(iface, config);

    unordered_map<uint16_t, Timestamp> icmpMap;
    struct timeval elapsed;
    bool printElapsed = false;
    string pduType;
    for (const auto packet : *sniffer) {
        printElapsed = false;

        if (const ICMP *icmp = packet.pdu()->find_pdu<ICMP>()) {
            pduType = "ICMP";
            if (icmp->type() == ICMP::ECHO_REQUEST) {
                pduType += " Request";
                icmpMap[icmp->id()] = packet.timestamp();
            } else if (icmp->type() == ICMP::ECHO_REPLY) {
                pduType += " Reply";
                Timestamp repTs = packet.timestamp();
                Timestamp reqTs = icmpMap[icmp->id()];
                icmpMap.erase(icmp->id());

                elapsed.tv_sec = repTs.seconds() - reqTs.seconds();
                if (repTs.microseconds() < reqTs.microseconds()) {
                    elapsed.tv_sec--;
                    elapsed.tv_usec = MILLION - reqTs.microseconds() + repTs.microseconds();
                } else {
                    elapsed.tv_usec = repTs.microseconds() - reqTs.microseconds();
                }

                printElapsed = true;
            }
        } else if (packet.pdu()->find_pdu<ARP>()) {
            pduType = "ARP";
        } else if (packet.pdu()->find_pdu<IP>()) {
            pduType = "IP";

            // Right now, assume only TCP & UDP above IP
            if (packet.pdu()->find_pdu<TCP>()) {
                pduType = "TCP";
            } else if (packet.pdu()->find_pdu<UDP>()) {
                pduType = "UDP";
            } else {
                pduType += "(Unknown transport protocol)";
            }
        } else {
            pduType = "Unknown";
        }

        cout << "Packet: " << pduType << " received at " <<
            packet.timestamp().seconds() << "." <<
            std::setw(6) << std::setfill('0') <<
            packet.timestamp().microseconds() << endl;

        if (printElapsed) {
            if (elapsed.tv_sec) // If >= 1 second
                cout << "Elapsed time: " << elapsed.tv_sec << "." <<
                    std::setw(6) << std::setfill('0') <<
                    elapsed.tv_usec << " s" << endl;
            else
                cout << "Elapsed time: " << (float)elapsed.tv_usec / THOUSAND << " ms" << endl;
        }

    }

    if (sniffer)
        delete sniffer;

    return 0;
}
