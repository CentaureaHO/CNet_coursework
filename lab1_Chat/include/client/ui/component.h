#ifndef __CLIENT_UI_COMPONENT_H__
#define __CLIENT_UI_COMPONENT_H__

#include <client/net/client.h>

#include <functional>
#include <string>
#include <memory>
#include <mutex>
#include <vector>

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>

class Page
{
  protected:
    static Client client_;

  public:
    static int current_page;

    virtual ftxui::Component GetComponent() = 0;
    virtual ~Page()                         = default;
};

class LoginPage : public Page
{
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

  public:
    LoginPage(std::function<void()> on_exit);

    ftxui::Component GetComponent() override;

    void SetError(const std::string& message);

    void ClearError();
    void ClearUsername();

    void Reset();
};

class HomePage : public Page
{
  private:
    ftxui::Component      logout_button_;
    ftxui::Component      exit_button_;
    ftxui::Component      container_;
    std::function<void()> on_logout_;
    std::function<void()> on_exit_;

  public:
    HomePage(std::function<void()> on_exit);
    ftxui::Component GetComponent();
};

class MainComponent : public ftxui::ComponentBase
{
  private:
    ftxui::ScreenInteractive&     screen_;
    int&                          selected_page_;
    ftxui::Component              container_;
    std::vector<ftxui::Component> pages_;
    std::shared_ptr<LoginPage>    login_page_;
    std::shared_ptr<HomePage>     home_page_;
    std::function<void()>         on_exit_;

  public:
    MainComponent(ftxui::ScreenInteractive& screen);

    bool           OnEvent(ftxui::Event event);
    ftxui::Element Render() override;
};

#endif