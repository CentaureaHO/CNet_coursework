#include <client/ui/component.h>
using namespace std;

HomePage::HomePage(MainComponent* m, std::function<void()> on_exit) : on_exit_(on_exit)
{
    main_component = m;
    using namespace ftxui;

    on_logout_ = [this]() {
        endListen();
        output_lines_.clear();
        client_.disconnect();
        client_.stop();
        current_page = 0;
    };

    logout_button_ = Button("Logout", on_logout_);
    exit_button_   = Button("Exit", [this]() {
        on_logout_();
        on_exit_();
    });

    input_content_ = "";
    input_box_     = Input(&input_content_, "Enter message");

    send_button_ = Button("Send", [this]() {
        client_.sendMessage(input_content_);
        input_content_.clear();
    });

    input_container_ = Container::Horizontal({
        input_box_,
        send_button_,
    });

    buttons_container_ = Container::Horizontal({
        logout_button_,
        exit_button_,
    });

    output_renderer_ = Renderer([&] {
        using namespace ftxui;
        Elements elements;
        for (const auto& line : output_lines_) { elements.push_back(text(line)); }
        auto content = vbox(std::move(elements));

        content = content | focusPositionRelative(0.0f, scroll_position_) | frame | vscroll_indicator;

        return content;
    });

    container_ = Container::Vertical({
        input_container_,
        buttons_container_,
    });
}

ftxui::Component HomePage::GetComponent()
{
    using namespace ftxui;

    return Renderer(container_, [&] {
        std::string window_title = " " + main_component->getUserName() + " ";

        auto content = output_renderer_->Render() | size(HEIGHT, EQUAL, 10);

        return vbox({
                   window(text(window_title), content),
                   separator(),
                   hbox({
                       text("Input: "),
                       input_box_->Render() | flex,
                       send_button_->Render(),
                   }),
                   filler(),
                   hbox({
                       logout_button_->Render(),
                       exit_button_->Render(),
                   }),
               }) |
               border;
    });
}

void HomePage::startListen()
{
    listening_ = true;
    client_.startListening([this](const string& message) {
        output_lines_.push_back(message);
        main_component->refresh();
    });
}

void HomePage::endListen()
{
    main_component->getUserName();
    client_.sendMessage("/disconnect");
    client_.disconnect();
    client_.stop();
}