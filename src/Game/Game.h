#pragma once

#include "GameObjects.h"

struct CoordsHash {
  inline size_t operator()(const app::Coords& coords) const {
    return std::hash<int>()(coords.x) ^ (std::hash<int>()(coords.y) << 1) ^ (std::hash<int>()(coords.z) << 2);
  }
};

namespace app {

class Application;

class Game {
public:
  constexpr Game() noexcept = default;
  Game(const Game&) = delete;
  constexpr Game(Game&&) noexcept = default;
  constexpr ~Game() noexcept = default;

  void Update(const GameState& game_state);

  Game& operator=(const Game&) = delete;
  constexpr Game& operator=(Game&&) noexcept = default;

private:
  void UpdateInfo(Application& app);

  void CountWordSizes();

  void PrintWords(std::string_view words_filename);
  void PrintStatistics(std::string_view stat_filename);

private:
  int current_turn_ = -1;
  std::vector<std::string> words_;
  std::vector<size_t> word_sizes_;
};

}  // namespace app