#include <net/rudp/rudp.h>
#include <common/lock.h>
#include <random>
#include <iostream>
#include <thread>
#include <cassert>
#include <iomanip>
#include <common/log.h>
using namespace std;

#define GUESS_RTT 50

void printRUDP(RUDP_P& p)
{
    p.body[p.header.data_len] = '\0';
    cout << "connect_id: " << p.header.connect_id << endl;
    cout << "body: " << p.body << endl;
}

RUDP::RUDP(int local_port)
    : _statu(RUDP_STATUS::CLOSED),
      _port(local_port),
      _sockfd(INVALID_SOCKET),
      _connect_id(0),
      _seq_num(0),
      _ack_num(0),
      _rtt(GUESS_RTT),
      _receiving(false)
{
    _sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    if (_sockfd == INVALID_SOCKET)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt)) < 0)
    {
        perror("setsockopt failed");
        CLOSE_SOCKET(_sockfd);
        exit(EXIT_FAILURE);
    }

    memset(&_local_addr, 0, sizeof(_local_addr));
    _local_addr.sin_family      = AF_INET;
    _local_addr.sin_addr.s_addr = INADDR_ANY;
    _local_addr.sin_port        = htons(_port);

    if (::bind(_sockfd, (const struct sockaddr*)&_local_addr, sizeof(_local_addr)) == SOCKET_ERROR)
    {
        perror("Bind failed");
        CLOSE_SOCKET(_sockfd);
        exit(EXIT_FAILURE);
    }
}
RUDP::~RUDP() {}

int RUDP::getBoundPort() const { return _port; }