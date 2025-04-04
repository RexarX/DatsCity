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

  enum class State { kConnected, kDisconnected, kWaitingForNextGame };

  Server() = default;
  Server(const Server&) = delete;
  Server(Server&&) noexcept = delete;
  ~Server() { Disconnect(); }

  void Connect(std::string_view url, std::string_view token);
  void Disconnect() { state_ = State::kDisconnected; }

  void Update();

  std::expected<TowersResponse, Error> FetchTowers();
  std::expected<WordsResponse, Error> FetchWords();
  std::expected<ShuffleResponse, Error> ShuffleWords() const;
  std::expected<RoundsResponse, Error> FetchRounds();
  std::expected<ShuffleResponse, Error> BuildTower(const BuildRequest& request);

  void PrintGameState();

  Server& operator=(const Server&) = delete;
  Server& operator=(Server&&) noexcept = delete;

  inline State GetState() const noexcept { return state_; }
  inline const std::string& GetUrl() const noexcept { return base_url_; }
  inline const std::string& GetToken() const noexcept { return token_; }
  inline const GameState& GetGameState() const noexcept { return game_state_; }

private:
  template <typename T>
  std::expected<std::string, Error> SendRequest(std::string_view endpoint, const T& data);
  std::expected<std::string, Error> SendGetRequest(std::string_view endpoint) const;
  inline std::string FormatEndpointUrl(std::string_view endpoint) const noexcept {
    return std::format("{}{}", base_url_, endpoint);
  }

  void ParseNoActiveGameError(const std::string& error_message);

private:
  State state_ = State::kDisconnected;
  Error last_error_;

  std::string base_url_;
  std::string token_;

  GameState game_state_;

  std::chrono::system_clock::time_point next_game_check_time_;

  cpr::Session session_;
};

template <typename T>
std::expected<std::string, Server::Error> Server::SendRequest(std::string_view endpoint, const T& data) {
  if (state_ == State::kDisconnected) {
    return std::unexpected(Error{.error = "Server is not connected"});
  }

  if (state_ == State::kWaitingForNextGame) {
    return std::unexpected(Error{.error = "No active game"});
  }

  std::string json;
  const auto err = glz::write_json(data, json);
  if (err) {
    return std::unexpected(Error{.error = std::format("Failed to serialize data: {}", glz::format_error(err, ""))});
  }

  INFO("Sending request to {}: {}", endpoint, json);

  cpr::Session req;
  req.SetUrl(FormatEndpointUrl(endpoint));
  req.SetHeader({{"X-Auth-Token", token_}, {"Content-Type", "application/json"}});
  req.SetBody(json);

  const cpr::Response response = req.Post();

  if (response.error) {
    return std::unexpected(Error{.error = std::format("Request failed: {}", response.error.message)});
  }

  if (response.status_code != 200) {
    ErrorResponse api_error;
    const auto parse_err = glz::read_json(api_error, response.text);
    if (parse_err) {
      return std::unexpected(Error{.error = std::format("HTTP error {}: {}", response.status_code, response.text)});
    }
    return std::unexpected(Error{.error = std::format("Error code {}: {}", api_error.code, api_error.message)});
  }

  return response.text;
}

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