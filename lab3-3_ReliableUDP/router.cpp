#include <net/socket_defs.h>
#include <common/log.h>
#include <common/lock.h>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <random>
#include <chrono>
#include <sstream>
using namespace std;

static const int BUF_SIZE = 65536;

static atomic<int>  g_delay_ms{10};  // 延迟时间(ms)
static atomic<int>  g_loss_rate{3};  // 丢包率百分比(0-100)
static atomic<bool> g_stop{false};   // 程序是否结束标志

Logger logger("router.log");

struct Packet
{
    vector<char>                     data;
    sockaddr_in                      dest;
    size_t                           size;
    chrono::steady_clock::time_point enqueue_time;
};

class PacketQueue
{
  public:
    void push(Packet&& pkt)
    {
        {
            lock_guard<mutex> lk(m_);
            q_.push(std::move(pkt));
        }
        cv_.notify_all();
    }

    bool try_front(Packet& pkt)
    {
        lock_guard<mutex> lk(m_);
        if (q_.empty()) return false;
        pkt = q_.front();
        return true;
    }

    void pop()
    {
        lock_guard<mutex> lk(m_);
        if (!q_.empty()) q_.pop();
    }

  private:
    queue<Packet>      q_;
    mutex              m_;
    condition_variable cv_;
};

static mt19937_64                    rng(random_device{}());
static uniform_int_distribution<int> dist_0_100(0, 100);

int main()
{
    SocketInitializer::getInstance();

    int router_port = 5000;

    const char* A_IP   = "127.0.0.1";
    int         A_PORT = 7777;
    sockaddr_in addrA;
    memset(&addrA, 0, sizeof(addrA));
    addrA.sin_family      = AF_INET;
    addrA.sin_addr.s_addr = inet_addr(A_IP);
    addrA.sin_port        = htons(A_PORT);

    const char* B_IP   = "127.0.0.1";
    int         B_PORT = 8888;
    sockaddr_in addrB;
    memset(&addrB, 0, sizeof(addrB));
    addrB.sin_family      = AF_INET;
    addrB.sin_addr.s_addr = inet_addr(B_IP);
    addrB.sin_port        = htons(B_PORT);

    SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == INVALID_SOCKET)
    {
        perror("socket create failed");
        exit(EXIT_FAILURE);
    }

    sockaddr_in local_addr;
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family      = AF_INET;
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    local_addr.sin_port        = htons(router_port);

    if (::bind(sock, (sockaddr*)&local_addr, sizeof(local_addr)) == SOCKET_ERROR)
    {
        perror("bind failed");
        CLOSE_SOCKET(sock);
        exit(EXIT_FAILURE);
    }

    LOG(logger, "Router started, listening on port ", router_port);
    LOG(logger, "Packets from A(", A_IP, ":", A_PORT, ") forward to B(", B_IP, ":", B_PORT, ")");
    LOG(logger, "Packets from B(", B_IP, ":", B_PORT, ") forward to A(", A_IP, ":", A_PORT, ")");
    LOG(logger, "Initial delay=", g_delay_ms.load(), "ms, loss_rate=", g_loss_rate.load(), "%");

    PacketQueue pkt_queue;

    auto send_thread_func = [&]() {
        while (!g_stop.load())
        {
            Packet pkt;
            if (!pkt_queue.try_front(pkt))
            {
                this_thread::sleep_for(chrono::milliseconds(1));
                continue;
            }

            int  delay = g_delay_ms.load();
            auto now   = chrono::steady_clock::now();
            if (now - pkt.enqueue_time >= chrono::milliseconds(delay))
            {
                int loss = g_loss_rate.load();
                int rnd  = dist_0_100(rng);
                if (rnd < loss) { LOG_WARN(logger, "Packet dropped, size=", pkt.size, ", loss_rate=", loss, "%"); }
                else
                {
                    int sret = sendto(sock, pkt.data.data(), (int)pkt.size, 0, (sockaddr*)&pkt.dest, sizeof(pkt.dest));
                    if (sret < 0)
                    {
                        perror("sendto error");
                        LOG_ERR(logger, "sendto error");
                    }
                    else
                    {
                        LOG(logger, "Packet sent, size=", pkt.size, ", delay=", delay, "ms, loss_rate=", loss, "%");
                    }
                }
                pkt_queue.pop();
            }
            else { this_thread::sleep_for(chrono::milliseconds(1)); }
        }

        LOG(logger, "Send thread exiting.");
    };

    auto receive_thread_func = [&]() {
        char buf[BUF_SIZE];
        while (!g_stop.load())
        {
            sockaddr_in src;
            socklen_t   srclen = sizeof(src);
            int         ret    = recvfrom(sock, buf, BUF_SIZE, 0, (sockaddr*)&src, &srclen);

            if (g_stop.load()) break;

            if (ret <= 0)
            {
                if (ret < 0)
                {
                    perror("recvfrom error");
                    LOG_ERR(logger, "recvfrom error");
                }
                continue;
            }

            bool fromA = (src.sin_addr.s_addr == addrA.sin_addr.s_addr && src.sin_port == addrA.sin_port);
            bool fromB = (src.sin_addr.s_addr == addrB.sin_addr.s_addr && src.sin_port == addrB.sin_port);

            sockaddr_in dest;
            if (fromA) { dest = addrB; }
            else if (fromB) { dest = addrA; }
            else
            {
                LOG_WARN(logger,
                    "Unknown source ",
                    inet_ntoa(src.sin_addr),
                    ":",
                    ntohs(src.sin_port),
                    ", packet discarded before queue.");
                continue;
            }

            Packet pkt;
            pkt.data.assign(buf, buf + ret);
            pkt.size         = ret;
            pkt.dest         = dest;
            pkt.enqueue_time = chrono::steady_clock::now();

            pkt_queue.push(std::move(pkt));
            // LOG(logger, "Packet queued, size=", ret, ", will send after delay & loss check");
        }

        LOG(logger, "Receive thread exiting.");
    };

    cout << "Control thread started. Commands: 'delay X', 'loss X', 'quit'" << endl;

    thread send_thread(send_thread_func);
    thread receive_thread(receive_thread_func);

    while (true)
    {
        string line;
        if (!getline(cin, line)) break;
        istringstream iss(line);
        string        cmd;
        iss >> cmd;
        if (cmd == "delay")
        {
            int d;
            if (iss >> d)
            {
                g_delay_ms.store(d);
                LOG(logger, "Delay changed to ", d, "ms");
            }
            else { LOG_ERR(logger, "Invalid delay value"); }
        }
        else if (cmd == "loss")
        {
            int l;
            if (iss >> l && l >= 0 && l <= 100)
            {
                g_loss_rate.store(l);
                LOG(logger, "Loss rate changed to ", l, "%");
            }
            else { LOG_ERR(logger, "Invalid loss value, must be 0-100"); }
        }
        else if (cmd == "quit")
        {
            LOG(logger, "Quit command received, stopping...");
            g_stop.store(true);
            break;
        }
        else { LOG_WARN(logger, "Unknown command: ", cmd); }
    }

    send_thread.join();
    receive_thread.join();

    CLOSE_SOCKET(sock);
    SOCKCLEANUP();
    LOG(logger, "Router stopped gracefully.");
    return 0;
}