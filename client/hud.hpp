#pragma once

#include <SFML/Graphics.hpp>
#include <string>

class Hud {
public:
    struct Style {
        float panelPadding = 12.f;
        float panelGap = 10.f;
        float barWidth = 220.f;
        float barHeight = 18.f;
    };

public:
    Hud() = default;
    explicit Hud(const sf::Font& font);

    void setFont(const sf::Font& font);
    void setWindowSize(const sf::Vector2f& size);
    void setStyle(const Style& style);

    void setPlayerName(const std::string& name);
    void setHealth(float current, float maximum);
    void setStamina(float current, float maximum);
    void setAmmo(int current, int reserve);
    void setPingMs(int pingMs);
    void setFps(float fps);
    void setCenterMessage(const std::string& text);
    void clearCenterMessage();

    void draw(sf::RenderTarget& target) const;

private:
    void updateLayout();
    void updateTexts();
    static float safeRatio(float current, float maximum);

private:
    const sf::Font* m_font = nullptr;
    Style m_style;

    sf::Vector2f m_windowSize = {1280.f, 720.f};

    std::string m_playerName = "Player";
    float m_healthCurrent = 100.f;
    float m_healthMax = 100.f;
    float m_staminaCurrent = 100.f;
    float m_staminaMax = 100.f;
    int m_ammoCurrent = 30;
    int m_ammoReserve = 90;
    int m_pingMs = 0;
    float m_fps = 0.f;
    std::string m_centerMessage;

    // Background panel
    sf::RectangleShape m_infoPanel;

    // Labels
    sf::Text m_nameText;
    sf::Text m_healthLabel;
    sf::Text m_staminaLabel;
    sf::Text m_ammoText;
    sf::Text m_pingText;
    sf::Text m_fpsText;
    sf::Text m_centerText;

    // Bars
    sf::RectangleShape m_healthBack;
    sf::RectangleShape m_healthFill;
    sf::RectangleShape m_staminaBack;
    sf::RectangleShape m_staminaFill;
};
