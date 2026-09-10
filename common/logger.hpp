#pragma once

#include "common/common.hpp"

#include <SFML/System/Vector2.hpp>

#include <ostream>
#include <sstream>
#include <string>

// ============================================================
// Data types you want to log
// ============================================================

std::ostream& operator<<(std::ostream& os, const sf::Vector2f& v);

template <typename T> std::ostream& operator<<(std::ostream& os, const sf::Rect<T>& rect) {
    os << "(x=" << rect.position.x << ", y=" << rect.position.y << ", w= " << rect.size.x
       << ", h=" << rect.size.y << ")";
    return os;
}

std::ostream& operator<<(std::ostream& os, const common::PlayerState& p);

std::ostream& operator<<(std::ostream& os, const common::InputCommand& cmd);

namespace common {

// ============================================================
// Logger
// ============================================================

/*
    Logger logger;
    common::PlayerState playertest;
    playertest.name = "Rick";
    playertest.pos = {100.f, 200.f};
    playertest.score = 42;
    logger.info() << "Game started";
    logger.info() << "Player state: " << playertest;
    logger.warn() << "Health dropping: " << playertest.health;
    logger.log_error("Snapshot: ", playertest);
*/

class Logger {
  public:
    enum class Level { Info, Warn, Error };

    explicit Logger(std::ostream& out = std::cout);

    // --------------------------------------------------------
    // Streaming log line helper
    // --------------------------------------------------------
    class LogLine {
      public:
        LogLine(Logger& logger, Level level);
        ~LogLine();

        LogLine(const LogLine&) = delete;
        LogLine& operator=(const LogLine&) = delete;

        LogLine(LogLine&& other) noexcept;

        template <typename T> LogLine& operator<<(const T& value) {
            stream_ << value;
            return *this;
        }

        using Manip = std::ostream& (*)(std::ostream&);

        LogLine& operator<<(Manip manip) {
            stream_ << manip;
            return *this;
        }

      private:
        Logger& logger_;
        Level level_;
        std::ostringstream stream_;
        bool committed_ = false;
    };

    [[nodiscard]] LogLine info();
    [[nodiscard]] LogLine warn();
    [[nodiscard]] LogLine error();

    // --------------------------------------------------------
    // Convenience fold-based logging
    // --------------------------------------------------------
    template <typename... Args> void log_info(Args&&... args) {
        write_fold(Level::Info, std::forward<Args>(args)...);
    }

    template <typename... Args> void log_warn(Args&&... args) {
        write_fold(Level::Warn, std::forward<Args>(args)...);
    }

    template <typename... Args> void log_error(Args&&... args) {
        write_fold(Level::Error, std::forward<Args>(args)...);
    }

  private:
    std::ostream& out_;

    static const char* level_to_string(Level level);
    static std::string make_timestamp();

    void write_line(Level level, const std::string& message);

    template <typename... Args> void write_fold(Level level, Args&&... args) {
        std::ostringstream oss;
        (oss << ... << std::forward<Args>(args));
        write_line(level, oss.str());
    }
};

} // namespace common
