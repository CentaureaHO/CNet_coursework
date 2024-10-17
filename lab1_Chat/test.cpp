// main.cpp
#include <iostream>

#include <atomic>
#include <common/net/socket_defs.h>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

// 定义基类 Page
class Page {
 public:
  virtual ftxui::Component GetComponent() = 0;
  virtual ~Page() = default;
};

// 登录页面 LoginPage
class LoginPage : public Page {
 public:
  LoginPage(std::function<void()> on_login, std::function<void()> on_exit)
      : on_login_(on_login), on_exit_(on_exit) {
    using namespace ftxui;

    // 输入框
    ip_address_ = "127.0.0.1";
    port_ = "8080";
    ip_input_ = Input(&ip_address_, "IP Address");
    port_input_ = Input(&port_, "Port");
    // 限制端口输入只能是数字
    port_input_ |= CatchEvent([&](Event event) {
      return event.is_character() && !std::isdigit(event.character()[0]);
    });
    username_input_ = Input(&username_, "Username");

    // 按钮
    login_button_ = Button("Login", on_login_);
    exit_button_ = Button("Exit", on_exit_);

    // 容器
    component_ = Container::Vertical({
        ip_input_,
        port_input_,
        username_input_,
        login_button_,
        exit_button_,
    });

    // 渲染器
    renderer_ = Renderer(component_, [&] {
      return vbox({
                 text("Login Page") | bold | center,
                 separator(),
                 hbox(text("IP Address: "), ip_input_->Render()),
                 hbox(text("Port      : "), port_input_->Render()),
                 hbox(text("Username  : "), username_input_->Render()),
                 separator(),
                 (error_message_.empty() ? text("") : text(error_message_) | color(ftxui::Color::Red)),
                 hbox({
                     login_button_->Render() | flex,
                     exit_button_->Render() | flex,
                 }) | center,
             }) |
             border;
    });
  }

  ftxui::Component GetComponent() override { return renderer_; }

  // 获取输入的数据
  std::string GetIPAddress() const { return ip_address_; }
  std::string GetPort() const { return port_; }
  std::string GetUsername() const { return username_; }

  void SetError(const std::string& message) { error_message_ = message; }

  void ClearError() { error_message_.clear(); }
  void ClearUsername() { username_.clear(); }

  void Reset() {
    ip_address_ = "127.0.0.1";
    port_ = "8080";
    username_.clear();
    ClearError();
  }

 private:
  std::string ip_address_;
  std::string port_;
  std::string username_;
  std::string error_message_;

  ftxui::Component ip_input_;
  ftxui::Component port_input_;
  ftxui::Component username_input_;
  ftxui::Component login_button_;
  ftxui::Component exit_button_;

  ftxui::Component component_;
  ftxui::Component renderer_;

  std::function<void()> on_login_;
  std::function<void()> on_exit_;
};

// 主页 HomePage
class HomePage : public Page {
 public:
  HomePage(std::function<void()> on_logout,
           std::function<void()> on_exit,
           SOCKET client_socket,
           ftxui::ScreenInteractive& screen)
      : on_logout_(on_logout),
        on_exit_(on_exit),
        client_socket_(client_socket),
        screen_(screen),
        running_(true) {
    using namespace ftxui;

    // 按钮
    logout_button_ = Button("Logout", [&] {
      StopClient();
      on_logout_();
    });
    exit_button_ = Button("Exit", [&] {
      StopClient();
      on_exit_();
    });

    // 输入框和发送按钮
    input_box_ = Input(&input_text_, "Type your message...");
    send_button_ = Button("Send", [&] {
      SendMessage();
    });

    // 容器
    component_ = Container::Vertical({
        input_box_,
        send_button_,
        logout_button_,
        exit_button_,
    });

    // 渲染器
    renderer_ = Renderer(component_, [&] {
      Elements messages_elements;
      {
        std::lock_guard<std::mutex> lock(messages_mutex_);
        for (const auto& msg : messages_) {
          messages_elements.push_back(text(msg));
        }
      }
      return vbox({
                 text("Home Page") | bold | center,
                 separator(),
                 vbox(std::move(messages_elements)) | vscroll_indicator | frame | size(HEIGHT, LESS_THAN, 20),
                 separator(),
                 hbox({
                     text("Message: "),
                     input_box_->Render(),
                     send_button_->Render(),
                 }),
                 separator(),
                 hbox({
                     logout_button_->Render() | flex,
                     exit_button_->Render() | flex,
                 }) | center,
             }) |
             border;
    });

    // 开始监听线程
    listening_thread_ = std::thread(&HomePage::ListenForMessages, this);
  }

  ~HomePage() {
    StopClient();
  }

  ftxui::Component GetComponent() override { return renderer_; }

  // 设置用户数据
  void SetUserData(const std::string& ip_address,
                   const std::string& port,
                   const std::string& username) {
    ip_address_ = ip_address;
    port_ = port;
    username_ = username;
  }

 private:
  // 用户数据
  std::string ip_address_;
  std::string port_;
  std::string username_;

  // 网络相关
  SOCKET client_socket_;
  ftxui::ScreenInteractive& screen_;
  std::atomic<bool> running_;
  std::thread listening_thread_;
  std::mutex messages_mutex_;

  // 消息列表
  std::vector<std::string> messages_;

  // 组件
  ftxui::Component logout_button_;
  ftxui::Component exit_button_;
  ftxui::Component input_box_;
  ftxui::Component send_button_;
  std::string input_text_;

  ftxui::Component component_;
  ftxui::Component renderer_;

  // 回调函数
  std::function<void()> on_logout_;
  std::function<void()> on_exit_;

  // 发送消息
  void SendMessage() {
    std::string message = input_text_;
    if (!message.empty()) {
      // 在单独的线程中发送消息
      std::thread([this, message]() {
        send(client_socket_, message.c_str(), message.length(), 0);
      }).detach();

      input_text_.clear();

      // 将自己的消息添加到消息列表
      {
        std::lock_guard<std::mutex> lock(messages_mutex_);
        messages_.push_back(username_ + ": " + message);
      }
      // 刷新界面
      screen_.PostEvent(ftxui::Event::Custom);
    }
  }

  // 监听服务器消息
  void ListenForMessages() {
    try {
      char buffer[1024];
      while (running_) {
        int valread = recv(client_socket_, buffer, sizeof(buffer) - 1, 0);
        if (valread <= 0) {
          // 连接关闭或发生错误
          if (running_) {
            // 仅在非主动停止时显示错误消息
            {
              std::lock_guard<std::mutex> lock(messages_mutex_);
              messages_.push_back("Server disconnected or error occurred.");
            }
            screen_.PostEvent(ftxui::Event::Custom);
          }
          break;
        }
        buffer[valread] = '\0';
        std::string message = std::string(buffer);
        // 将消息添加到消息列表
        {
          std::lock_guard<std::mutex> lock(messages_mutex_);
          messages_.push_back(message);
        }
        // 刷新界面
        screen_.PostEvent(ftxui::Event::Custom);
      }
    } catch (const std::exception& e) {
      // 处理异常
      std::cerr << "Exception in ListenForMessages: " << e.what() << std::endl;
    }
  }

  // 停止客户端
  void StopClient() {
    if (running_) {
      running_ = false;
      // 关闭套接字以使 recv() 解除阻塞
      CLOSE_SOCKET(client_socket_);
      if (listening_thread_.joinable()) {
        listening_thread_.join();
      }
    }
  }
};

// 主组件 MainComponent
class MainComponent : public ftxui::ComponentBase {
 public:
  MainComponent(ftxui::ScreenInteractive& screen)
      : screen_(screen), selected_page_(0) {
    using namespace ftxui;

    on_exit_ = [&] { screen_.ExitLoopClosure()(); };

    // 创建登录页面
    login_page_ = std::make_shared<LoginPage>(
        [&]() {
          // 将登录逻辑移到新线程中
          std::thread(&MainComponent::AttemptLogin, this).detach();
        },
        on_exit_);

    // 创建主页（先创建一个占位的空组件）
    home_page_ = std::make_shared<HomePage>(
        []() {}, on_exit_, INVALID_SOCKET, screen_);

    // 初始化页面数组
    pages_ = {
        login_page_->GetComponent(),
        home_page_->GetComponent(),
    };

    // 创建 Tab 容器
    container_ = Container::Tab(pages_, &selected_page_);

    Add(container_);
  }

  // 处理事件
  bool OnEvent(ftxui::Event event) override { return ComponentBase::OnEvent(event); }

  // 渲染当前组件
  ftxui::Element Render() override { return container_->Render(); }

 private:
  ftxui::ScreenInteractive& screen_;
  int selected_page_;
  ftxui::Component container_;
  std::vector<ftxui::Component> pages_;
  std::shared_ptr<LoginPage> login_page_;
  std::shared_ptr<HomePage> home_page_;
  std::function<void()> on_exit_;
  std::string ip_address_;
  std::string port_;
  std::string username_;

  SOCKET client_socket_;

  // 尝试登录的方法
  void AttemptLogin() {
    ip_address_ = login_page_->GetIPAddress();
    port_ = login_page_->GetPort();
    username_ = login_page_->GetUsername();

    if (username_.empty()) {
      login_page_->SetError("用户名不能为空。");
      screen_.PostEvent(ftxui::Event::Custom);
      return;
    }

    client_socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket_ == INVALID_SOCKET) {
      login_page_->SetError("无法创建套接字。");
      screen_.PostEvent(ftxui::Event::Custom);
      return;
    }
    sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(std::stoi(port_));
    if (inet_pton(AF_INET, ip_address_.c_str(), &serv_addr.sin_addr) <= 0 ||
        connect(client_socket_, (sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
      login_page_->SetError("无法连接到服务器，请检查 IP 地址和端口号。");
      screen_.PostEvent(ftxui::Event::Custom);
      return;
    }

    char buffer[1024];
    int valread = recv(client_socket_, buffer, 1024, 0);
    if (valread <= 0) {
      login_page_->SetError("无法接收服务器响应。");
      screen_.PostEvent(ftxui::Event::Custom);
      return;
    }
    buffer[valread] = '\0';
    if (std::string(buffer) != "notfull") {
      login_page_->SetError("服务器人数达到上限，请稍后再试。");
      screen_.PostEvent(ftxui::Event::Custom);
      return;
    }
    send(client_socket_, username_.c_str(), username_.size(), 0);
    valread = recv(client_socket_, buffer, 1024, 0);
    if (valread <= 0) {
      login_page_->SetError("无法接收服务器响应。");
      screen_.PostEvent(ftxui::Event::Custom);
      return;
    }
    buffer[valread] = '\0';
    std::stringstream ss(buffer);
    std::string rv;
    ss >> rv;
    if (rv != "accepted") {
      login_page_->SetError("用户名已被占用，请更换。");
      login_page_->ClearUsername();
      screen_.PostEvent(ftxui::Event::Custom);
      return;
    }
    ss >> port_;

    // 登录成功，更新 UI
    screen_.Post([this]() {
      // 设置主页的用户数据
      home_page_->SetUserData(ip_address_, port_, username_);
      // 初始化主页，传递 client_socket 和 screen
      home_page_ = std::make_shared<HomePage>(
          [&]() {
            // 登出回调
            login_page_->Reset();              // 清空登录页面的数据
            selected_page_ = 0;                // 切换回登录页面
            screen_.PostEvent(ftxui::Event::Custom);  // 触发界面刷新
          },
          on_exit_, client_socket_, screen_);

      pages_[1] = home_page_->GetComponent();
      selected_page_ = 1;                // 切换到主页
      screen_.PostEvent(ftxui::Event::Custom);  // 触发界面刷新
    });
  }
};

// 主函数
int main() {
  auto screen = ftxui::ScreenInteractive::TerminalOutput();

  auto main_component = std::make_shared<MainComponent>(screen);

  screen.Loop(main_component);

  return 0;
}
