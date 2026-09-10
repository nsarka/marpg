#pragma once
#include "client_connection.hpp"
#include <atomic>
#include <chrono>
#include <exception>
#include <thread>

// Exclusive socket ownership while the render thread loads the level. Join the
// worker before normal gameplay touches the connection or received snapshots.
class LoadingConnection {
  public:
    explicit LoadingConnection(ClientConnection& connection)
        : connection_(connection), worker_([this] {
              try {
                  while (!stop_.load()) {
                      connection_.pumpNetwork(states_, joined_);
                      std::this_thread::sleep_for(std::chrono::milliseconds(20));
                  }
              } catch (...) {
                  error_ = std::current_exception();
              }
          }) {}
    ~LoadingConnection() {
        stop();
    }
    LoadingConnection(const LoadingConnection&) = delete;
    LoadingConnection& operator=(const LoadingConnection&) = delete;
    void finish(std::vector<common::PlayerState>& states, std::vector<common::PlayerId>& joined) {
        stop();
        if (error_)
            std::rethrow_exception(error_);
        states = std::move(states_);
        joined = std::move(joined_);
    }

  private:
    void stop() {
        stop_ = true;
        if (worker_.joinable())
            worker_.join();
    }
    ClientConnection& connection_;
    std::atomic<bool> stop_{false};
    std::vector<common::PlayerState> states_ = std::vector<common::PlayerState>(common::MAX_PLAYERS);
    std::vector<common::PlayerId> joined_;
    std::exception_ptr error_;
    std::thread worker_;
};
