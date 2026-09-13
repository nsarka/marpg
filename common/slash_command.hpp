#pragma once
#include <algorithm>
#include <cctype>
#include <charconv>
#include <optional>
#include <string>
namespace common {
struct SlashCommand {
    std::string name, argument;
};
inline std::optional<SlashCommand> slashCommand(const std::string& text) {
    if (text.empty() || text[0] != '/')
        return {};
    auto end = text.find_first_of(" \t", 1);
    SlashCommand result{text.substr(1, end == std::string::npos ? end : end - 1), {}};
    std::transform(result.name.begin(), result.name.end(), result.name.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    if (end != std::string::npos) {
        auto first = text.find_first_not_of(" \t", end);
        if (first != std::string::npos)
            result.argument = text.substr(first, text.find_last_not_of(" \t") - first + 1);
    }
    return result;
}
inline std::optional<unsigned> commandNumber(const std::string& text) {
    unsigned value = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size())
        return {};
    return value;
}
} // namespace common
