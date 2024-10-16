#include <ftxui/component/component.hpp>           // for Menu
#include <ftxui/component/screen_interactive.hpp>  // for ScreenInteractive
#include <ftxui/dom/elements.hpp>                  // for text, Element, hbox, vbox, gauge
#include <ftxui/screen/color.hpp>                  // for Color, Color::Red, Color::Blue

int main()
{
    using namespace ftxui;

    // 定义一个简单的 UI 布局
    auto screen   = ScreenInteractive::TerminalOutput();
    auto renderer = Renderer([] {
        return vbox({
            hbox({
                text("Label 1") | border,
                text("Label 2") | border | flex,
                text("Label 3") | border | flex,
            }),
            gauge(0.3) | color(Color::Red),
            gauge(0.6) | color(Color::Blue),
            gauge(0.9) | color(Color::Green),
        });
    });

    // 启动屏幕并展示界面
    screen.Loop(renderer);

    return 0;
}
