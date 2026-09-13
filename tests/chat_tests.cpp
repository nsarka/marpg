#include "client/chat_input.hpp"
#include "common/settings.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
void check(bool ok, const char* message) {
    if (!ok)
        throw std::runtime_error(message);
}
void press(ChatInput& input, sf::Keyboard::Key key) {
    sf::Event::KeyPressed event{};
    event.code = key;
    input.consume(event);
}
void release(ChatInput& input, sf::Keyboard::Key key) {
    sf::Event::KeyReleased event{};
    event.code = key;
    input.consume(event);
}
void type(ChatInput& input, char32_t c) {
    input.consume(sf::Event::TextEntered{c});
}
int main() {
    using K = sf::Keyboard::Key;
    ChatInput input;
    press(input, K::Enter);
    check(input.open(), "Enter did not open chat");
    press(input, K::Enter);
    check(input.open() && !input.takeSubmitted(), "Key repeat submitted chat");
    type(input, 13);
    release(input, K::Enter);
    type(input, U'h');
    type(input, U'i');
    type(input, U'é');
    press(input, K::Backspace);
    release(input, K::Backspace);
    check(input.draft() == sf::String("hi"), "Unicode backspace erased wrong character");
    press(input, K::Enter);
    check(input.takeSubmitted() == std::optional<std::string>("hi") && !input.open(), "Send failed");
    release(input, K::Enter);
    press(input, K::Enter);
    release(input, K::Enter);
    type(input, U'x');
    press(input, K::Escape);
    check(!input.open() && !input.takeSubmitted(), "Escape sent message");

    common::KeyBindings bindings;
    bindings[common::Action::Chat] = {common::parseBinding("T")};
    ChatInput rebound(bindings);
    press(rebound, K::Enter);
    check(!rebound.open(), "Enter still opened remapped chat");
    release(rebound, K::Enter);
    press(rebound, K::T);
    type(rebound, U't');
    release(rebound, K::T);
    check(rebound.draft().isEmpty(), "Opening key appeared in draft");
    press(rebound, K::W);
    type(rebound, U'w');
    release(rebound, K::W);
    press(rebound, K::Enter);
    check(rebound.takeSubmitted() == std::optional<std::string>("w"), "Remapping changed send key");
    bindings[common::Action::Chat].clear();
    ChatInput disabled(bindings);
    press(disabled, K::Enter);
    check(!disabled.open(), "Disabled chat opened");
    ChatInput bounded;
    press(bounded, K::Enter);
    release(bounded, K::Enter);
    for (int i = 0; i < 300; ++i)
        type(bounded, U'z');
    check(bounded.draft().getSize() == common::ChatMaxCharacters, "Draft is unbounded");
    check(common::cleanChatText(" \n hi\t \r") == "hi", "Control characters survived");
    check(common::cleanChatText(std::string(513, 'x')).empty(), "Oversized text accepted");
    common::PlayerState player;
    player.name = "Alice";
    player.team = 1;
    player.alive = false;
    player.deaths = 3;
    auto message = common::playerChat(8, 0, player, "hello");
    check(common::chatLine(message) == "*DEAD* Alice: hello", "Dead prefix missing");
    auto packet = common::chatPacket(message);
    std::string tag;
    packet >> tag;
    common::ChatMessage decoded;
    check(common::readChat(packet, decoded) && decoded.dead && decoded.deaths == 3 && decoded.text == "hello",
          "Chat wire round trip failed");
    common::ChatInbox inbox;
    check(inbox.accept(2) && inbox.accept(1) && !inbox.accept(2), "Reordering or duplicate filtering failed");
    common::ChatOutbox outbox;
    check(outbox.enqueue(1, packet, 0), "Queue failed");
    check(outbox.due(0).size() == 1 && outbox.due(100).empty() && outbox.due(250).size() == 1,
          "Retry timing incorrect");
    outbox.acknowledge(1);
    check(outbox.due(500).empty(), "ACK did not stop retries");
    outbox.enqueue(2, packet, 500);
    check(outbox.expire(15500) == 1, "Expired messages retained");
    auto config = std::filesystem::temp_directory_path() / "marpg-chat-bindings.toml";
    std::ofstream(config) << "[bindings]\nchat=\"T\"\n";
    check(common::loadClientSettings(config.string()).bindings[common::Action::Chat][0] ==
              common::parseBinding("T"),
          "Chat TOML binding failed");
    std::ofstream(config) << "[bindings]\nchat=[]\n";
    check(common::loadClientSettings(config.string()).bindings[common::Action::Chat].empty(),
          "Chat TOML disable failed");
    std::filesystem::remove(config);
    std::cout << "Chat input, protocol, retry, and configuration checks passed\n";
}
