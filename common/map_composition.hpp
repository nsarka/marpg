#pragma once
#include "load_progress.hpp"
#include <string>
namespace common {
// Expand authored prefabs/groups/tile objects before the shared TMX consumers run.
std::string composeMap(const std::string& path, LoadProgress* progress = nullptr);
} // namespace common
