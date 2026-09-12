#pragma once
#include <chrono>
#include <cstddef>
#include <functional>
#include <stdexcept>
#include <string>
#include <utility>
namespace common {
class LoadingCancelled : public std::runtime_error {
  public:
    LoadingCancelled() : std::runtime_error("Loading cancelled") {}
};
struct LoadStatus {
    std::string stage;
    std::size_t completed = 0, total = 0;
    double stageSeconds = 0, totalSeconds = 0;
    bool finished = false;
};
// Callbacks run on the loading thread. report() checks cancellation even when
// progress output is throttled; a zero total means the amount of work is unknown.
class LoadProgress {
    using Clock = std::chrono::steady_clock;
    std::function<void(const LoadStatus&)> output_;
    std::function<bool()> cancelled_;
    Clock::time_point started_ = Clock::now(), stageStarted_ = started_, lastOutput_{};
    LoadStatus status_;
    bool active_ = false;
    void emit(bool finished) {
        const auto now = Clock::now();
        status_.stageSeconds = std::chrono::duration<double>(now - stageStarted_).count();
        status_.totalSeconds = std::chrono::duration<double>(now - started_).count();
        status_.finished = finished;
        if (output_)
            output_(status_);
        lastOutput_ = now;
    }

  public:
    LoadProgress(std::function<void(const LoadStatus&)> output = {}, std::function<bool()> cancelled = {})
        : output_(std::move(output)), cancelled_(std::move(cancelled)) {}
    void check() const {
        if (cancelled_ && cancelled_())
            throw LoadingCancelled();
    }
    void report(const std::string& stage, std::size_t completed = 0, std::size_t total = 0) {
        check();
        const bool changed = !active_ || stage != status_.stage;
        if (changed) {
            finish();
            status_ = {stage, completed, total};
            stageStarted_ = Clock::now();
            active_ = true;
        } else {
            status_.completed = completed;
            status_.total = total;
        }
        if (changed || Clock::now() - lastOutput_ >= std::chrono::milliseconds(100))
            emit(false);
    }
    void finish() {
        if (active_) {
            emit(true);
            active_ = false;
        }
    }
};
inline void loading(LoadProgress* progress, const std::string& stage, std::size_t done = 0,
                    std::size_t total = 0) {
    if (progress)
        progress->report(stage, done, total);
}
} // namespace common
