#include "hud.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>


Hud::Hud(const sf::Font& font)
: m_font(&font),
  m_nameText(font, "", 22),
  m_healthLabel(font, "", 16),
  m_staminaLabel(font, "", 16),
  m_ammoText(font, "", 20),
  m_pingText(font, "", 16),
  m_fpsText(font, "", 16),
  m_centerText(font, "", 28)
{
    updateTexts();
    updateLayout();
}

void Hud::setFont(const sf::Font& font) {
    m_font = &font;

    updateTexts();
    updateLayout();
}

void Hud::setWindowSize(const sf::Vector2f& size) {
    m_windowSize = size;
    updateLayout();
}

void Hud::setStyle(const Style& style) {
    m_style = style;
    updateLayout();
}

void Hud::setPlayerName(const std::string& name) {
    m_playerName = name;
    updateTexts();
}

void Hud::setHealth(float current, float maximum) {
    m_healthCurrent = current;
    m_healthMax = maximum;
    updateTexts();
    updateLayout();
}

void Hud::setStamina(float current, float maximum) {
    m_staminaCurrent = current;
    m_staminaMax = maximum;
    updateTexts();
    updateLayout();
}

void Hud::setAmmo(int current, int reserve) {
    m_ammoCurrent = current;
    m_ammoReserve = reserve;
    updateTexts();
}

void Hud::setPingMs(int pingMs) {
    m_pingMs = pingMs;
    updateTexts();
}

void Hud::setFps(float fps) {
    m_fps = fps;
    updateTexts();
}

void Hud::setCenterMessage(const std::string& text) {
    m_centerMessage = text;
    updateTexts();
    updateLayout();
}

void Hud::clearCenterMessage() {
    m_centerMessage.clear();
    updateTexts();
    updateLayout();
}

void Hud::updateTexts() {
    if (!m_font) {
        return;
    }

    m_nameText.setString(m_playerName);

    {
        std::ostringstream oss;
        oss << "Health  " << static_cast<int>(std::round(m_healthCurrent))
            << " / " << static_cast<int>(std::round(m_healthMax));
        m_healthLabel.setString(oss.str());
    }

    {
        std::ostringstream oss;
        oss << "Stamina " << static_cast<int>(std::round(m_staminaCurrent))
            << " / " << static_cast<int>(std::round(m_staminaMax));
        m_staminaLabel.setString(oss.str());
    }

    {
        std::ostringstream oss;
        oss << "Ammo " << m_ammoCurrent << " / " << m_ammoReserve;
        m_ammoText.setString(oss.str());
    }

    {
        std::ostringstream oss;
        oss << "Ping " << m_pingMs << " ms";
        m_pingText.setString(oss.str());
    }

    {
        std::ostringstream oss;
        oss << "FPS " << static_cast<int>(std::round(m_fps));
        m_fpsText.setString(oss.str());
    }

    m_centerText.setString(m_centerMessage);
}

float Hud::safeRatio(float current, float maximum) {
    if (maximum <= 0.f) {
        return 0.f;
    }
    return std::clamp(current / maximum, 0.f, 1.f);
}

void Hud::updateLayout() {
    const float pad = m_style.panelPadding;
    const float gap = m_style.panelGap;

    // Top-left panel
    m_infoPanel.setPosition({pad, pad});
    m_infoPanel.setSize({310.f, 150.f});
    m_infoPanel.setFillColor(sf::Color(0, 0, 0, 150));
    m_infoPanel.setOutlineThickness(1.f);
    m_infoPanel.setOutlineColor(sf::Color(255, 255, 255, 40));

    m_nameText.setPosition({pad + 14.f, pad + 10.f});

    m_healthLabel.setPosition({pad + 14.f, pad + 42.f});
    m_healthBack.setPosition({pad + 14.f, pad + 68.f});
    m_healthBack.setSize({m_style.barWidth, m_style.barHeight});
    m_healthBack.setFillColor(sf::Color(40, 40, 40, 220));
    m_healthBack.setOutlineThickness(1.f);
    m_healthBack.setOutlineColor(sf::Color(255, 255, 255, 30));

    m_healthFill.setPosition(m_healthBack.getPosition());
    m_healthFill.setSize({
        m_style.barWidth * safeRatio(m_healthCurrent, m_healthMax),
        m_style.barHeight
    });
    m_healthFill.setFillColor(sf::Color(200, 70, 70));

    m_staminaLabel.setPosition({pad + 14.f, pad + 92.f});
    m_staminaBack.setPosition({pad + 14.f, pad + 118.f});
    m_staminaBack.setSize({m_style.barWidth, m_style.barHeight});
    m_staminaBack.setFillColor(sf::Color(40, 40, 40, 220));
    m_staminaBack.setOutlineThickness(1.f);
    m_staminaBack.setOutlineColor(sf::Color(255, 255, 255, 30));

    m_staminaFill.setPosition(m_staminaBack.getPosition());
    m_staminaFill.setSize({
        m_style.barWidth * safeRatio(m_staminaCurrent, m_staminaMax),
        m_style.barHeight
    });
    m_staminaFill.setFillColor(sf::Color(80, 180, 110));

    // Bottom-right readouts
    const auto ammoBounds = m_ammoText.getLocalBounds();
    m_ammoText.setPosition({
        m_windowSize.x - ammoBounds.size.x - 20.f,
        m_windowSize.y - 52.f
    });

    const auto pingBounds = m_pingText.getLocalBounds();
    m_pingText.setPosition({
        m_windowSize.x - pingBounds.size.x - 20.f,
        14.f
    });

    const auto fpsBounds = m_fpsText.getLocalBounds();
    m_fpsText.setPosition({
        m_windowSize.x - fpsBounds.size.x - 20.f,
        36.f
    });

    // Center message
    const auto centerBounds = m_centerText.getLocalBounds();
    m_centerText.setOrigin({
        centerBounds.position.x + centerBounds.size.x * 0.5f,
        centerBounds.position.y + centerBounds.size.y * 0.5f
    });
    m_centerText.setPosition({
        m_windowSize.x * 0.5f,
        m_windowSize.y * 0.35f
    });
}

void Hud::draw(sf::RenderTarget& target) const {
    target.draw(m_infoPanel);
    target.draw(m_nameText);

    target.draw(m_healthLabel);
    target.draw(m_healthBack);
    target.draw(m_healthFill);

    target.draw(m_staminaLabel);
    target.draw(m_staminaBack);
    target.draw(m_staminaFill);

    target.draw(m_ammoText);
    target.draw(m_pingText);
    target.draw(m_fpsText);

    if (!m_centerMessage.empty()) {
        target.draw(m_centerText);
    }
}
