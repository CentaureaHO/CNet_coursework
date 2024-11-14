#include <bits/stdc++.h>
#include <net/packet.h>
using namespace std;

int main()
{
    RUPacket packet;
    packet.header.flags = 0x00;
    packet.header.flags |= SYN;
    packet.header.pid = 0xFFAC00FF;
    packet.header.cid = CID(0x00000000, 0x000000AC);

    packet.data       = new char[4096];
    packet.header.dlh = 0x00;
    packet.header.dlm = 0x00;
    packet.header.dll = 0x03;
    packet.data[0]    = 'H';
    packet.data[1]    = 'H';
    packet.data[2]    = 'H';
    set_sum_check(packet);

    printf("Sum check: %x\n", packet.header.sum_check);
    printf("Check sum check: %d\n", check_sum_check(packet));

    packet.data[2]    = 'h';
    printf("Check sum check: %d\n", check_sum_check(packet));

    delete[] packet.data;
}