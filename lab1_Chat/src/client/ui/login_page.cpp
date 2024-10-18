#include <client/ui/component.h>
using namespace std;

LoginPage::LoginPage(MainComponent* m, std::function<void()> on_exit) : on_exit_(on_exit)
{
    main_component = m;
    using namespace ftxui;

    on_login_ = [this]() {
        if (username_.empty())
        {
            error_message_ = "Username cannot be empty!";
            return;
        }
        client_.setTarget(ip_address_, stoi(port_));
        if (!client_.connectToServer(error_message_) || !client_.login(username_, error_message_))
        {
            client_.disconnect();
            current_page = 0;
            return;
        }
        current_page = 1;
        error_message_.clear();
        main_component->startListen();
    };

    ip_address_ = "127.0.0.1";
    port_       = "8080";
    ip_input_   = Input(&ip_address_, "IP Address");
    port_input_ = Input(&port_, "Port");
    port_input_ |= CatchEvent([&](Event event) { return event.is_character() && !std::isdigit(event.character()[0]); });
    username_input_ = Input(&username_, "Username");

    login_button_ = Button("Login", on_login_);
    exit_button_  = Button("Exit", on_exit_);

    component_ = Container::Vertical({
        ip_input_,
        port_input_,
        username_input_,
        login_button_,
        exit_button_,
    });

    renderer_ = Renderer(component_, [&] {
        using namespace ftxui;
        return vbox({
                   text("Login Page") | bold | center,
                   separator(),
                   hbox(text("IP Address: "), ip_input_->Render()),
                   hbox(text("Port      : "), port_input_->Render()),
                   hbox(text("Username  : "), username_input_->Render()),
                   separator(),
                   (error_message_.empty() ? text("") : text(error_message_) | color(Color::Red)),
                   hbox({
                       login_button_->Render() | flex,
                       exit_button_->Render() | flex,
                   }) | center,
               }) |
               border;
    });
}

ftxui::Component LoginPage::GetComponent() { return renderer_; }

void LoginPage::SetError(const std::string& message) { error_message_ = message; }

void LoginPage::ClearError() { error_message_.clear(); }
void LoginPage::ClearUsername() { username_.clear(); }

void LoginPage::Reset()
{
    ip_address_ = "127.0.0.1";
    port_       = "8080";
    username_.clear();
    ClearError();
}