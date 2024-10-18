#include <iostream>

#include <server/net/message_dispatcher.h>
#include <common/console/format_date.h>
using namespace std;

queue<std::pair<std::string, ClientInfo>> MessageDispatcher::message_queue;
mutex                                     MessageDispatcher::queue_mutex;
condition_variable                        MessageDispatcher::queue_cond;
bool                                      MessageDispatcher::running = true;

void MessageDispatcher::dispatchMessages()
{
    while (true)
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        queue_cond.wait(lock, [] { return !message_queue.empty() || !running; });

        if (!running && message_queue.empty())
        {
            //cout << "Message dispatcher stopped." << endl;
            break;
        }

        auto [message, client_info] = message_queue.front();
        message_queue.pop();

        lock.unlock();

        if (send(client_info.c_socket, message.c_str(), message.size(), 0) == -1)
        {
            DERR << "Failed to send message to client " << client_info.c_nickname << endl;
        }
    }
}

void MessageDispatcher::sendToSingle(const string& sender, const string& message, ClientInfo& client_info)
{
    bool                         is_disconnect = (message == "/disconnect");
    string                       msg           = is_disconnect ? message : sender + formatDate() + ":  " + message;
    std::unique_lock<std::mutex> lock(queue_mutex);
    message_queue.push(make_pair(msg, client_info));
    queue_cond.notify_one();
}

void MessageDispatcher::sendToGroup(const string& sender, const string& message, GroupInfo& group, SOCKET self)
{
#ifdef _WIN32
    string msg = sender + " -> Group: " + group.group_name + ":\r\n\t" + message;
#else
    string msg = sender + " -> Group: " + group.group_name + ":\n\t" + message;
#endif
    std::unique_lock<std::mutex> lock(queue_mutex);
    for (ClientInfo* client : group.members)
    {
        if (client->c_socket == self) continue;
        message_queue.push(make_pair(msg, *client));
    }
    queue_cond.notify_one();
}

void MessageDispatcher::clearQueue()
{
    std::unique_lock<std::mutex> lock(queue_mutex);
    while (!message_queue.empty()) message_queue.pop();
    queue_cond.notify_all();
}

void MessageDispatcher::stop()
{
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        running = false;
    }
    queue_cond.notify_all();
}
