#pragma once

#include "GameObjects.h"
#include "pch.h"

#include <raylib.h>

struct CoordsHash {
  inline size_t operator()(const app::Coords& coords) const {
    return std::hash<int>()(coords.x) ^ (std::hash<int>()(coords.y) << 1) ^ (std::hash<int>()(coords.z) << 2);
  }
};

namespace app {

class Game {
public:
  constexpr Game() noexcept = default;
  Game(const Game&) = delete;
  constexpr Game(Game&&) noexcept = default;
  constexpr ~Game() noexcept = default;

  void Update(const GameState& gameState);

  Game& operator=(const Game&) = delete;
  constexpr Game& operator=(Game&&) noexcept = default;
};

}  // namespace app