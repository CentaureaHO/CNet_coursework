#include <net/rudp/rudp.h>
#include <common/lock.h>
#include <random>
#include <iostream>
#include <thread>
#include <cassert>
#include <iomanip>
#include <common/log.h>
using namespace std;

using us = chrono::microseconds;

namespace
{
    Logger server_log("server.log");
}  // namespace

#define SLOG(...) LOG(server_log, __VA_ARGS__)
#define SLOG_WARN(...) LOG_WARN(server_log, __VA_ARGS__)
#define SLOG_ERR(...) LOG_ERR(server_log, __VA_ARGS__)

RUDP_S::RUDP_S(int port) : RUDP(port) {}
RUDP_S::~RUDP_S()
{
    if (_sockfd != INVALID_SOCKET) CLOSE_SOCKET(_sockfd);
}

void RUDP_S::_receive_handler() {}

void RUDP_S::_listen() {}
void RUDP_S::_syn_rcvd() {}
void RUDP_S::_established(callback cb) {}
void RUDP_S::_fin_rcvd() {}

void RUDP_S::listen(callback cb) {}