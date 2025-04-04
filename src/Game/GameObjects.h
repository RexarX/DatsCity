#pragma once

#include "pch.h"

#include <glaze/glaze.hpp>

namespace app {

struct Coords {
  int x = 0;
  int y = 0;
  int z = 0;

  constexpr Coords& operator+=(const Coords& other) noexcept {
    x += other.x;
    y += other.y;
    z += other.z;
    return *this;
  }

  constexpr Coords& operator-=(const Coords& other) noexcept {
    x -= other.x;
    y -= other.y;
    z -= other.z;
    return *this;
  }

  constexpr bool operator==(const Coords& other) const noexcept { return x == other.x && y == other.y && z == other.z; }
};

constexpr Coords operator+(const Coords& lhs, const Coords& rhs) noexcept {
  return Coords{.x = lhs.x + rhs.x, .y = lhs.y + rhs.y, .z = lhs.z + rhs.z};
}

constexpr Coords operator-(const Coords& lhs, const Coords& rhs) noexcept {
  return Coords{
      .x = lhs.x - rhs.x,
      .y = lhs.y - rhs.y,
      .z = lhs.z - rhs.z,
  };
}

enum class Direction {
  kUp = 1,      // [0, 0, -1]
  kRight = 2,   // [1, 0, 0]
  kForward = 3  // [0, 1, 0]
};

constexpr Coords DirectionToCoords(Direction dir) noexcept {
  switch (dir) {
    case Direction::kUp:
      return {0, 0, -1};
    case Direction::kRight:
      return {1, 0, 0};
    case Direction::kForward:
      return {0, 1, 0};
    default:
      return {0, 0, 0};
  }
}

// Word structures
struct WordRequest {
  Coords pos{0, 0, 0};
  Direction dir;
  size_t id = 0;
};

struct WordResponse {
  std::string text;
};

struct WordPlaced {
  std::string text;
  Direction dir;
  Coords pos;
};

// Build API structures
struct BuildRequest {
  bool done = false;
  std::vector<WordRequest> words;
};

struct ShuffleResponse {
  int shuffle_left = 0;
  std::vector<std::string> words;
};

// Words API structures
struct WordsResponse {
  Coords map_size{0, 0, 0};
  int next_turn_sec = 0;
  std::string round_ends_at;
  int shuffle_left = 0;
  int turn = 0;
  std::vector<size_t> used_indexes;
  std::vector<std::string> words;
};

// Tower structures
struct DoneTower {
  int id = 0;
  double score = 0;
};

struct Tower {
  double score = 0;
  std::vector<WordPlaced> words;
};

// Towers API structures
struct TowersResponse {
  std::vector<DoneTower> done_towers;
  double score = 0;
  std::optional<Tower> tower;
};

// Rounds API structures
struct Round {
  std::string end_at;
  std::string name;
  int repeat = 0;
  int duration = 0;
  std::string start_at;
  std::string status;
};

struct RoundsResponse {
  std::string event_id;
  std::string now;
  std::vector<Round> rounds;
};

// Error response structure
struct ErrorResponse {
  int code = 0;
  std::string message;
};

struct GameState {
  WordsResponse words_data;
  TowersResponse towers_data;
  std::vector<Round> rounds_data;
};

}  // namespace app

template <>
struct glz::meta<app::Coords> {
  using T = app::Coords;
  static constexpr auto value = array(&T::x, &T::y, &T::z);
};

template <>
struct glz::meta<app::WordRequest> {
  using T = app::WordRequest;
  static constexpr auto value = object("id", &T::id, "dir", &T::dir, "pos", &T::pos);
};

template <>
struct glz::meta<app::WordResponse> {
  using T = app::WordResponse;
  static constexpr auto value = object("text", &T::text);
};

template <>
struct glz::meta<app::WordPlaced> {
  using T = app::WordPlaced;
  static constexpr auto value = object("text", &T::text, "dir", &T::dir, "pos", &T::pos);
};

template <>
struct glz::meta<app::BuildRequest> {
  using T = app::BuildRequest;
  static constexpr auto value = object("done", &T::done, "words", &T::words);
};

template <>
struct glz::meta<app::ShuffleResponse> {
  using T = app::ShuffleResponse;
  static constexpr auto value = object("shuffleLeft", &T::shuffle_left, "words", &T::words);
};

template <>
struct glz::meta<app::WordsResponse> {
  using T = app::WordsResponse;
  static constexpr auto value =
      object("mapSize", &T::map_size, "nextTurnSec", &T::next_turn_sec, "roundEndsAt", &T::round_ends_at, "shuffleLeft",
             &T::shuffle_left, "turn", &T::turn, "usedIndexes", &T::used_indexes, "words", &T::words);
};

template <>
struct glz::meta<app::DoneTower> {
  using T = app::DoneTower;
  static constexpr auto value = object("id", &T::id, "score", &T::score);
};

template <>
struct glz::meta<app::Tower> {
  using T = app::Tower;
  static constexpr auto value = object("score", &T::score, "words", &T::words);
};

template <>
struct glz::meta<app::TowersResponse> {
  using T = app::TowersResponse;
  static constexpr auto value = object("doneTowers", &T::done_towers, "score", &T::score, "tower", &T::tower);
};

template <>
struct glz::meta<app::Round> {
  using T = app::Round;
  static constexpr auto value = object("duration", &T::duration, "endAt", &T::end_at, "name", &T::name, "repeat",
                                       &T::repeat, "startAt", &T::start_at, "status", &T::status);
};

template <>
struct glz::meta<app::RoundsResponse> {
  using T = app::RoundsResponse;
  static constexpr auto value = object("eventId", &T::event_id, "now", &T::now, "rounds", &T::rounds);
};

template <>
struct glz::meta<app::ErrorResponse> {
  using T = app::ErrorResponse;
  static constexpr auto value = object("code", &T::code, "message", &T::message);
};

template <>
struct glz::meta<app::Direction> {
  static constexpr std::string_view name = "Direction";
  static constexpr auto value = glz::enumerate("kUp", app::Direction::kUp, "kRight", app::Direction::kRight, "kForward",
                                               app::Direction::kForward);
};