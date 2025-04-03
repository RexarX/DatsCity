#pragma once

#include "pch.h"

#include <glaze/glaze.hpp>

namespace app {

struct Coords {
  int x = 0;
  int y = 0;

  constexpr Coords& operator+=(const Coords& other) noexcept {
    x += other.x;
    y += other.y;
    return *this;
  }

  constexpr Coords& operator-=(const Coords& other) noexcept {
    x -= other.x;
    y -= other.y;
    return *this;
  }

  constexpr bool operator==(const Coords& other) const noexcept { return x == other.x && y == other.y; }
};

constexpr Coords operator+(const Coords& lhs, const Coords& rhs) noexcept {
  return Coords{.x = lhs.x + rhs.x, .y = lhs.y + rhs.y};
}

constexpr Coords operator-(const Coords& lhs, const Coords& rhs) noexcept {
  return Coords{
      .x = lhs.x - rhs.x,
      .y = lhs.y - rhs.y,
  };
}

struct Enemy {
  std::string status = "dead";
  int kills = 0;
};

struct Player {
  std::string id;
  std::string status = "dead";
  int death_count = 0;
  int revive_remain_ms = 0;
};

struct GameState {
  Coords map_size{0, 0};
  std::string name;
  std::vector<Player> players;
  std::vector<Enemy> enemies;
  int points = 0;
  int turn = 0;
  int tick_remain_ms = 0;
  int revive_timeout_sec = 0;
  std::vector<std::string> errors;
};

}  // namespace app

template <>
struct glz::meta<app::Coords> {
  using T = app::Coords;
  static constexpr auto value = array(&T::x, &T::y);
};

template <>
struct glz::meta<app::Enemy> {
  using T = app::Enemy;
  static constexpr auto value = object("status", &T::status, "kills", &T::kills);
};

template <>
struct glz::meta<app::Player> {
  using T = app::Player;
  static constexpr auto value =
      object("id", &T::id, "deathCount", &T::death_count, "status", &T::status, "reviveRemainMs", &T::revive_remain_ms);
};

template <>
struct glz::meta<app::GameState> {
  using T = app::GameState;
  static constexpr auto value =
      object("mapSize", &T::map_size, "name", &T::name, "points", &T::points, "players", &T::players, "enemies",
             &T::enemies, "turn", &T::turn, "tickRemainMs", &T::tick_remain_ms, "reviveTimeoutSec",
             &T::revive_timeout_sec, "errors", &T::errors);
};