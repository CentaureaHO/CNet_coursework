#include <client/ui/component.h>
using namespace std;

HomePage::HomePage(std::function<void()> on_exit) : on_exit_(on_exit)
{
    using namespace ftxui;

    on_logout_ = [this]() {
        client_.disconnect();
        client_.stop();
        current_page = 0;
    };

    logout_button_ = Button("Logout", on_logout_);
    exit_button_   = Button("Exit", on_exit_);

    container_ = Container::Vertical({
        logout_button_,
        exit_button_,
    });
}

ftxui::Component HomePage::GetComponent()
{
    using namespace ftxui;
    return Renderer(container_, [&] {
        return vbox({
                   text("Home Page") | bold | center,
                   separator(),
                   logout_button_->Render() | center,
                   exit_button_->Render() | center,
               }) |
               border;
    });
}