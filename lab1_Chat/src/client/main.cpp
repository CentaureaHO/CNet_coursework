#include <bits/stdc++.h>

#include <client/ui/component.h>
using namespace std;

int main()
{
    auto screen = ftxui::ScreenInteractive::TerminalOutput();

    auto main_component = std::make_shared<MainComponent>(screen);

    screen.Loop(main_component);

    return 0;
}