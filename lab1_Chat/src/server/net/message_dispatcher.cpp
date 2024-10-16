#include <iostream>

#include <server/net/message_dispatcher.h>
using namespace std;

queue<std::pair<std::string, ClientInfo>> MessageDispatcher::message_queue;
mutex                                 MessageDispatcher::queue_mutex;
condition_variable                    MessageDispatcher::queue_cond;

void MessageDispatcher::dispatchMessages()
{
    while (true)
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        queue_cond.wait(lock, [] { return !message_queue.empty(); });

        auto [message, client_info] = message_queue.front();
        message_queue.pop();

        lock.unlock();

        if (send(client_info.c_socket, message.c_str(), message.size(), 0) == -1)
        {
            cerr << "Failed to send message to client " << client_info.c_nickname << endl;
        }
    }
}
#include <iostream>
void MessageDispatcher::sendToSingle(const string& sender, const string& message, ClientInfo& client_info)
{
#ifdef _WIN32
    string msg = sender + " -> me:\r\n\t" + message;
#else
    string msg = sender + " -> me:\n\t" + message;
#endif
    std::unique_lock<std::mutex> lock(queue_mutex);
    std::cout << "Sending message to " << client_info.c_nickname << std::endl;
    message_queue.push(make_pair(msg, client_info));
    std::cout << "Message sent to " << client_info.c_nickname << std::endl;
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