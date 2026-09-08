#pragma once
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/Mouse.hpp>
#include <array>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace common {
inline constexpr std::array<const char*,8> BindingNames={"move_up","move_down","move_left","move_right","walk","jab","hook","scoreboard"};
// Keyboard codes occupy the first range; mouse buttons follow them.
inline int mouseBinding(sf::Mouse::Button button){return int(sf::Keyboard::KeyCount)+int(button);}
inline int parseBinding(std::string name) {
    std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return std::tolower(c);});
    using K=sf::Keyboard::Key;
    if(name.size()==1 && name[0]>='a' && name[0]<='z')return int(K::A)+name[0]-'a';
    if(name.size()==1 && name[0]>='0' && name[0]<='9')return int(K::Num0)+name[0]-'0';
    for(int i=1;i<=15;++i)if(name=="f"+std::to_string(i))return int(K::F1)+i-1;
    for(int i=0;i<=9;++i)if(name=="numpad"+std::to_string(i))return int(K::Numpad0)+i;
    const std::pair<const char*,K> keys[]={
        {"escape",K::Escape},{"tab",K::Tab},{"space",K::Space},{"enter",K::Enter},{"backspace",K::Backspace},
        {"lshift",K::LShift},{"rshift",K::RShift},{"lcontrol",K::LControl},{"rcontrol",K::RControl},
        {"lalt",K::LAlt},{"ralt",K::RAlt},{"lsystem",K::LSystem},{"rsystem",K::RSystem},
        {"up",K::Up},{"down",K::Down},{"left",K::Left},{"right",K::Right},
        {"insert",K::Insert},{"delete",K::Delete},{"home",K::Home},{"end",K::End},
        {"pageup",K::PageUp},{"pagedown",K::PageDown},{"pause",K::Pause},{"menu",K::Menu},
        {"comma",K::Comma},{"period",K::Period},{"slash",K::Slash},{"backslash",K::Backslash},
        {"semicolon",K::Semicolon},{"apostrophe",K::Apostrophe},{"grave",K::Grave},
        {"equal",K::Equal},{"hyphen",K::Hyphen},{"lbracket",K::LBracket},{"rbracket",K::RBracket},
        {"add",K::Add},{"subtract",K::Subtract},{"multiply",K::Multiply},{"divide",K::Divide}};
    for(auto [label,key]:keys)if(name==label)return int(key);
    const std::pair<const char*,sf::Mouse::Button> buttons[]={
        {"mouse_left",sf::Mouse::Button::Left},{"mouse_right",sf::Mouse::Button::Right},
        {"mouse_middle",sf::Mouse::Button::Middle},{"mouse_x1",sf::Mouse::Button::Extra1},{"mouse_x2",sf::Mouse::Button::Extra2}};
    for(auto [label,button]:buttons)if(name==label)return mouseBinding(button);
    throw std::runtime_error("Unknown keybinding: "+name);
}
struct KeyBindings {
    std::array<std::vector<int>,9> actions{{{parseBinding("w")},{parseBinding("s")},{parseBinding("a")},{parseBinding("d")},
        {parseBinding("lshift"),parseBinding("rshift")},{parseBinding("mouse_left")},{parseBinding("mouse_right")},
        {parseBinding("tab")},{parseBinding("f1")}}};
};
}
