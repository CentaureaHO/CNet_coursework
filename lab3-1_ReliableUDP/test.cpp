#include <bits/stdc++.h>
#include <net/rudp/rudp.h>
using namespace std;

namespace
{
    random_device                      rd;
    mt19937                            gen(rd());
    uniform_int_distribution<uint32_t> dist(0, UINT32_MAX);
}  // namespace

int main()
{
    uint32_t cid = dist(gen);

    RUDP_P syn_packet;
    syn_packet.header.connect_id = cid;
    SET_SYN(syn_packet);
    genCheckSum(syn_packet);
    cout << "Checksum of syn_packet is " << (checkCheckSum(syn_packet) ? "correct" : "incorrect") << endl;

    char data[] = "Hello world!";
    RUDP_P data_packet;
    data_packet.header.connect_id = cid;
    data_packet.header.data_len = sizeof(data);
    memcpy(data_packet.body, data, sizeof(data));
    genCheckSum(data_packet);
    data_packet.body[1] = 'a';  // 修改数据包内容，使其校验和不正确
    cout << "Checksum of data_packet is " << (checkCheckSum(data_packet) ? "correct" : "incorrect") << endl;
}