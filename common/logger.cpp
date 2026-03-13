#include "common/logger.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>

// ============================================================
// Struct stream operators (templated operators are defined in the header, not just declared)
// ============================================================

std::ostream& operator<<(std::ostream& os, const sf::Vector2f& v)
{
    os << "(" << v.x << ", " << v.y << ")";
    return os;
}

std::ostream& operator<<(std::ostream& os, const common::PlayerState& p)
{
    os << "PlayerState{"
       << "connected=" << std::boolalpha << p.connected
       << ", alive=" << std::boolalpha << p.alive
       << ", pos=" << p.pos
       << ", vel=" << p.vel
       << ", name=\"" << p.name << "\""
       << ", health=" << p.health
       << ", score=" << p.score
       << "}";

    return os;
}

namespace common {

// ============================================================
// Logger implementation
// ============================================================

Logger::Logger(std::ostream& out)
    : out_(out)
{
}

Logger::LogLine::LogLine(Logger& logger, Level level)
    : logger_(logger),
      level_(level)
{
}

Logger::LogLine::LogLine(LogLine&& other) noexcept
    : logger_(other.logger_),
      level_(other.level_),
      stream_(std::move(other.stream_)),
      committed_(other.committed_)
{
    other.committed_ = true;
}

Logger::LogLine::~LogLine()
{
    if (!committed_) {
        logger_.write_line(level_, stream_.str());
    }
}

Logger::LogLine Logger::info()
{
    return LogLine(*this, Level::Info);
}

Logger::LogLine Logger::warn()
{
    return LogLine(*this, Level::Warn);
}

Logger::LogLine Logger::error()
{
    return LogLine(*this, Level::Error);
}

const char* Logger::level_to_string(Level level)
{
    switch (level) {
        case Level::Info:  return "INFO";
        case Level::Warn:  return "WARN";
        case Level::Error: return "ERROR";
    }
    return "UNKNOWN";
}

std::string Logger::make_timestamp()
{
    using clock = std::chrono::system_clock;

    const auto now = clock::now();
    const auto time_t_now = clock::to_time_t(now);

    std::tm local_tm{};

#if defined(_WIN32)
    localtime_s(&local_tm, &time_t_now);
#else
    localtime_r(&time_t_now, &local_tm);
#endif

    const auto ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;

    std::ostringstream oss;

    oss << std::put_time(&local_tm, "%Y-%m-%d %H:%M:%S")
        << "."
        << std::setfill('0') << std::setw(3) << ms.count();

    return oss.str();
}

void Logger::write_line(Level level, const std::string& message)
{
    out_ << "["
         << make_timestamp()
         << "] ["
         << level_to_string(level)
         << "] "
         << message
         << '\n';
}

} // namespace common
