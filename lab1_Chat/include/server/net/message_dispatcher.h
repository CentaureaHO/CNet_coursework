#ifndef __SERVER_NET_MESSAGE_DISPATCHER_H__
#define __SERVER_NET_MESSAGE_DISPATCHER_H__

#include <condition_variable>
#include <string>
#include <queue>
#include <utility>
#include <mutex>

#include <common/net/socket_defs.h>
#include <server/net/session_manager.h>

class MessageDispatcher
{
  private:
    static std::queue<std::pair<std::string, ClientInfo>> message_queue;
    static std::mutex                                     queue_mutex;
    static std::condition_variable                        queue_cond;

  public:
    static void dispatchMessages();

    static void sendToSingle(const std::string& sender, const std::string& message, ClientInfo& client_info);
    static void sendToGroup(const std::string& sender, const std::string& message, GroupInfo& group, SOCKET self);

    static void clearQueue();
};

#endif