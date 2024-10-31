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

class MainComponent;

class Page
{
  protected:
    static Client client_;

  public:
    static int     current_page;
    MainComponent* main_component;

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
    LoginPage(MainComponent* m, std::function<void()> on_exit);

    ftxui::Component GetComponent() override;

    void SetError(const std::string& message);

    void ClearError();
    void ClearUsername();

    void        Reset();
    std::string getUserName() { return username_; }
};

class HomePage : public Page
{
  private:
    // Existing components
    ftxui::Component      logout_button_;
    ftxui::Component      exit_button_;
    std::function<void()> on_logout_;
    std::function<void()> on_exit_;

    std::string              input_content_;
    std::vector<std::string> output_lines_;
    ftxui::Component         input_box_;
    ftxui::Component         send_button_;
    ftxui::Component         output_renderer_;
    ftxui::Component         input_container_;
    ftxui::Component         buttons_container_;
    ftxui::Component         container_;
    bool                     listening_       = false;
    float                    scroll_position_ = 1.0f;

  public:
    HomePage(MainComponent* m, std::function<void()> on_exit);
    ftxui::Component GetComponent() override;

    void startListen();
    void endListen();
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

    bool           OnEvent(ftxui::Event event) override;
    ftxui::Element Render() override;

    std::string getUserName() { return login_page_->getUserName(); }
    void        startListen();

    void refresh();
    void errFeedback(const std::string& message);
};

#endif