#include <client/ui/component.h>

Client Page::client_(1024);
int    Page::current_page = 0;

MainComponent::MainComponent(ftxui::ScreenInteractive& screen) : screen_(screen), selected_page_(Page::current_page)
{
    using namespace ftxui;

    on_exit_ = [&] { screen_.ExitLoopClosure()(); };

    login_page_ = std::make_shared<LoginPage>(this, on_exit_);

    home_page_ = std::make_shared<HomePage>(this, on_exit_);

    pages_ = {
        login_page_->GetComponent(),
        home_page_->GetComponent(),
    };

    container_ = ftxui::Container::Tab(pages_, &selected_page_);

    Add(container_);
}

bool           MainComponent::OnEvent(ftxui::Event event) { return ComponentBase::OnEvent(event); }
ftxui::Element MainComponent::Render() { return container_->Render(); }

void MainComponent::startListen() { home_page_->startListen(); }

void MainComponent::refresh() { screen_.PostEvent(ftxui::Event::Custom); }