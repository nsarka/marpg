#include "client/chat_ui.hpp"
#include <filesystem>
int main(int argc, char** argv) {
    if (argc != 2)
        return 1;
    sf::Font font(std::filesystem::path(argv[1]) / "assets/fonts/PixelPurl.ttf");
    sf::RenderTexture target({1000, 700});
    ChatUI chat;
    common::PlayerState alice, bob;
    alice.connected = bob.connected = true;
    alice.name = "Alice";
    alice.team = 0;
    bob.name = "Bob";
    bob.team = 1;
    bob.alive = false;
    chat.add(common::playerChat(1, 0, alice, "Meet me at the north gate!"));
    chat.add(common::playerChat(2, 1, bob, "I found the switch. Wait for me to respawn."));
    common::ChatMessage announcement;
    announcement.kind = common::ChatKind::Announcement;
    announcement.name = "Server";
    announcement.text = "Round starts soon";
    chat.add(announcement);
    sf::Event::KeyPressed enter{};
    enter.code = sf::Keyboard::Key::Enter;
    chat.input.consume(enter);
    sf::Event::KeyReleased up{};
    up.code = sf::Keyboard::Key::Enter;
    chat.input.consume(up);
    for (auto c : sf::String("On my way"))
        chat.input.consume(sf::Event::TextEntered{c});
    target.clear(sf::Color(40, 48, 45));
    chat.drawAbove(target, font, 0, alice, {300, 270});
    chat.drawAbove(target, font, 1, bob, {730, 270});
    chat.draw(target, font, 2);
    target.display();
    const auto image = target.getTexture().copyToImage();
    unsigned livingPixels = 0, deadPixels = 0;
    for (unsigned y = 60; y < 140; ++y)
        for (unsigned x = 50; x < 950; ++x) {
            auto c = image.getPixel({x, y});
            if (c.r > 200 && c.g > 200 && c.b > 200) {
                if (x < 500)
                    ++livingPixels;
                else
                    ++deadPixels;
            }
        }
    if (!livingPixels || deadPixels)
        return 2;
    if (!image.saveToFile(std::filesystem::temp_directory_path() / "marpg-chat-preview.png"))
        return 3;
    alice.alive = false;
    chat.update(.1f, {alice, bob});
    alice.alive = true;
    ++alice.deaths;
    target.clear(sf::Color::Black);
    chat.drawAbove(target, font, 0, alice, {300, 270});
    target.display();
    const auto after = target.getTexture().copyToImage();
    for (unsigned y = 60; y < 140; ++y)
        for (unsigned x = 50; x < 500; ++x)
            if (after.getPixel({x, y}) != sf::Color::Black)
                return 4;
    // Names retain their team tint while message text, including wrapped lines,
    // is white. Keep this isolated from the draft and announcement text.
    ChatUI colors;
    colors.add(common::playerChat(
        3, 1, bob, "This message is long enough to wrap onto another line in the chat panel."));
    target.clear(sf::Color::Black);
    colors.draw(target, font, 2);
    target.display();
    const auto colored = target.getTexture().copyToImage();
    unsigned namePixels = 0, messagePixels = 0;
    for (unsigned y = 450; y < 615; ++y)
        for (unsigned x = 26; x < 480; ++x) {
            const auto pixel = colored.getPixel({x, y});
            const bool blue = pixel.b > 200 && pixel.r < 180 && pixel.g < 180;
            if (blue) {
                if (x > 180)
                    return 5;
                ++namePixels;
            }
            if (x > 180 && pixel.r > 230 && pixel.g > 230 && pixel.b > 230)
                ++messagePixels;
        }
    if (!namePixels || !messagePixels)
        return 6;
    return 0;
}
