#pragma once
#include "common/load_progress.hpp"
#include "common/loaded_map.hpp"
#include "common/logger.hpp"
#include <SFML/Graphics.hpp>
#include <atomic>
#include <future>
#include <iomanip>
#include <mutex>
#include <sstream>

// All window events and graphics stay on the main thread, including while the
// third-party XML parser is working. Cancellation is cooperative at file boundaries.
class LoadingScreen {
    sf::RenderWindow& window_;
    const sf::Font& font_;
    common::Logger& logger_;
    std::function<bool()> interrupted_;
    std::string lastLoggedStage_;
    double lastLoggedTime_ = -1;

  public:
    common::LoadProgress progress;
    LoadingScreen(sf::RenderWindow& window, const sf::Font& font, common::Logger& logger,
                  std::function<bool()> interrupted)
        : window_(window), font_(font), logger_(logger), interrupted_(std::move(interrupted)),
          progress([this](const auto& s) { draw(s); }, [this] { return cancelled(); }) {}
    bool cancelled() {
        while (auto event = window_.pollEvent()) {
            if (event->is<sf::Event::Closed>() ||
                (event->is<sf::Event::KeyPressed>() &&
                 event->getIf<sf::Event::KeyPressed>()->code == sf::Keyboard::Key::Escape))
                window_.close();
        }
        return !window_.isOpen() || interrupted_();
    }
    void draw(const common::LoadStatus& s) {
        if (s.finished || s.stage != lastLoggedStage_ || s.totalSeconds - lastLoggedTime_ >= 1) {
            logger_.info() << s.stage << (s.finished ? " complete" : "") << " (stage " << s.stageSeconds
                           << "s, total " << s.totalSeconds << "s)";
            logger_.flush();
            lastLoggedStage_ = s.stage;
            lastLoggedTime_ = s.totalSeconds;
        }
        if (!window_.isOpen())
            return;
        std::ostringstream message;
        message << "Loading MARPG\n\n" << s.stage;
        if (s.total)
            message << "\n" << s.completed << " / " << s.total;
        message << std::fixed << std::setprecision(1) << "\n\nStage: " << s.stageSeconds
                << "s    Total: " << s.totalSeconds << "s\n\nEscape to cancel";
        window_.setView(window_.getDefaultView());
        window_.clear(sf::Color(24, 26, 32));
        sf::Text text(font_, message.str(), 32);
        text.setPosition({48, 48});
        window_.draw(text);
        if (s.total) {
            sf::RectangleShape bar({500.f * std::min(1.f, float(s.completed) / s.total), 8});
            bar.setPosition({48, 350});
            bar.setFillColor(sf::Color(100, 170, 220));
            window_.draw(bar);
        }
        window_.display();
    }
    std::unique_ptr<common::LoadedMap> loadMap(const std::string& path) {
        std::atomic<bool> stop{false};
        std::mutex mutex;
        common::LoadStatus status{"Composing map"};
        auto task = std::async(std::launch::async, [&] {
            common::LoadProgress worker(
                [&](const auto& s) {
                    std::lock_guard<std::mutex> lock(mutex);
                    status = s;
                },
                [&] { return stop.load(); });
            return std::make_unique<common::LoadedMap>(path, &worker);
        });
        try {
            do {
                common::LoadStatus current;
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    current = status;
                }
                progress.report(current.stage, current.completed, current.total);
            } while (task.wait_for(std::chrono::milliseconds(50)) != std::future_status::ready);
            progress.check();
            return task.get();
        } catch (...) {
            stop = true;
            if (task.valid())
                task.wait(); // parser owns references to this scope until it returns
            throw;
        }
    }
};
