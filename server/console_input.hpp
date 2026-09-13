#pragma once
#include <iostream>
#include <string>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <SFML/System/String.hpp>
#include <windows.h>
#else
#include <poll.h>
#include <unistd.h>
#endif
// Poll stdin without blocking the simulation or leaving a getline thread alive
// during shutdown. Supports terminals, redirected files, and pipes.
class ConsoleInput {
    std::string pending_;
    bool ended_ = false, overflow_ = false;
    void append(char c, std::vector<std::string>& lines) {
        if (c == '\r')
            return;
        if (c == '\n') {
            lines.push_back(overflow_ ? "/input-too-long" : pending_);
            pending_.clear();
            overflow_ = false;
        } else if (pending_.size() < 2048)
            pending_ += c;
        else
            overflow_ = true;
    }

  public:
    std::vector<std::string> poll() {
        std::vector<std::string> lines;
        if (ended_)
            return lines;
        char buffer[1024];
#ifdef _WIN32
        HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
        if (input == INVALID_HANDLE_VALUE || !input) {
            ended_ = true;
            return lines;
        }
        DWORD mode = 0, available = 0, read = 0;
        if (GetConsoleMode(input, &mode)) {
            if (!GetNumberOfConsoleInputEvents(input, &available))
                return lines;
            for (DWORD i = 0; i < available && i < 256; ++i) {
                INPUT_RECORD event{};
                if (!ReadConsoleInputW(input, &event, 1, &read) || !read)
                    break;
                if (event.EventType != KEY_EVENT || !event.Event.KeyEvent.bKeyDown)
                    continue;
                const auto c = event.Event.KeyEvent.uChar.UnicodeChar;
                if (c == '\b') {
                    if (!pending_.empty()) {
                        auto last = pending_.size() - 1;
                        while (last > 0 && (static_cast<unsigned char>(pending_[last]) & 0xc0) == 0x80)
                            --last;
                        pending_.erase(last);
                        std::cout << "\b \b" << std::flush;
                    }
                } else if (c == '\r') {
                    append('\n', lines);
                    std::cout << '\n' << std::flush;
                } else if (c >= 32) {
                    auto bytes = sf::String(static_cast<char32_t>(c)).toUtf8();
                    for (auto b : bytes)
                        append(char(b), lines);
                    DWORD written;
                    WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), &c, 1, &written, nullptr);
                }
            }
            return lines;
        }
        const auto type = GetFileType(input);
        if (type == FILE_TYPE_PIPE) {
            if (!PeekNamedPipe(input, nullptr, 0, nullptr, &available, nullptr)) {
                ended_ = true;
                if (!pending_.empty())
                    append('\n', lines);
                return lines;
            }
            if (!available)
                return lines;
        }
        if (!ReadFile(input, buffer, sizeof(buffer), &read, nullptr) || !read)
            ended_ = true;
        else
            for (DWORD i = 0; i < read; ++i)
                append(buffer[i], lines);
#else
        pollfd input{STDIN_FILENO, POLLIN, 0};
        if (::poll(&input, 1, 0) <= 0)
            return lines;
        const auto read = ::read(STDIN_FILENO, buffer, sizeof(buffer));
        if (read == 0)
            ended_ = true;
        else if (read > 0)
            for (ssize_t i = 0; i < read; ++i)
                append(buffer[i], lines);
#endif
        if (ended_ && !pending_.empty()) {
            append('\n', lines);
        }
        return lines;
    }
};
