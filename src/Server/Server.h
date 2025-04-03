#pragma once

#include "Game/GameObjects.h"

#include <cpr/cpr.h>

namespace app {

class Server {
public:
  struct GameRound {
    std::string name;
    std::string start_time;
    std::string end_time;
  };

  struct Error {
    std::string error;
    int err_code = 0;
    std::string current_time;
    std::vector<GameRound> next_rounds;
  };

  enum class State { Connected, Disconnected, WaitingForNextGame };

  Server() = default;
  Server(const Server&) = delete;
  Server(Server&&) noexcept = delete;
  ~Server() { Disconnect(); }

  void Connect(std::string_view url, std::string_view token);
  void Disconnect();

  void Update();
  void Send(std::string_view json);

  void PrintGameState();

  Server& operator=(const Server&) = delete;
  Server& operator=(Server&&) noexcept = delete;

  inline State GetState() const noexcept { return state_; }

  inline const std::string& GetUrl() const noexcept { return url_; }
  inline const std::string& GetToken() const noexcept { return token_; }

  inline const GameState& GetGameState() const noexcept { return game_state_; }

private:
  State state_ = State::Disconnected;
  Error last_error_;

  std::string url_;
  std::string token_;

  GameState game_state_;

  cpr::Session session_;
};

}  // namespace app

template <>
struct glz::meta<app::Server::GameRound> {
  using T = app::Server::GameRound;
  static constexpr auto value = object("name", &T::name, "startTime", &T::start_time, "endTime", &T::end_time);
};

template <>
struct glz::meta<app::Server::Error> {
  using T = app::Server::Error;
  static constexpr auto value = object("error", &T::error, "errCode", &T::err_code, "currentTime", &T::current_time,
                                       "nextRounds", &T::next_rounds);
};