#include "Server.h"

constexpr const char* MOVE_ENDPOINT = "player/move";

namespace app {

void Server::Connect(std::string_view url, std::string_view token) {
  if (url.empty()) {
    ASSERT(false, "Failed to connect to the server: url is empty!");
    return;
  }

  if (token.empty()) {
    ASSERT(false, "Failed to connect to the server: token is empty!");
    return;
  }

  if (url == url_ && token == token_ && state_ == State::Connected) {
    return;
  }

  url_ = url;
  token_ = token;

  session_.SetUrl(std::format("{}/{}", url, MOVE_ENDPOINT));
  session_.SetHeader({{"X-Auth-Token", token_}, {"Content-Type", "application/json"}});

  state_ = State::Connected;
}

void Server::Disconnect() {
  state_ = State::Disconnected;
}

void Server::Update() {
  if (state_ == State::Disconnected) {
    ASSERT(false, "Failed to update server: server is not connected!");
    return;
  }

  const cpr::Response response = session_.Post();
  if (response.error) {
    ASSERT(false, "Failed to update server: {}!", response.error.message);
    return;
  }

  glz::error_ctx err = glz::read_json(game_state_, response.text);
  if (!err) {
    state_ = State::Connected;
    return;
  }

  ERROR("Error while updating server: Failed to parse server response: {}!", glz::format_error(err, response.text));

  err = glz::read_json(last_error_, response.text);
  if (err) {
    if (last_error_.err_code == 23) {  // No active game error
      state_ = State::WaitingForNextGame;
      if (!last_error_.next_rounds.empty()) {
        const GameRound& next_game = last_error_.next_rounds[0];
        INFO("No active game. Next game '{}' starts at {}", next_game.name, next_game.start_time);
      }
    }
    return;
  }

  ASSERT(false, "Failed to update server: Failed to parse server response: {}!", glz::format_error(err, response.text));
}

void Server::Send(std::string_view json) {
  if (state_ == State::Disconnected) {
    ASSERT(false, "Failed to post to the server: server is not connected!");
    return;
  }

  if (state_ == State::WaitingForNextGame) {
    return;
  }

  if (json.empty()) {
    ASSERT(false, "Failed to post to the server: json is empty!");
    return;
  }

  INFO("Sending json: {}", json);
  session_.SetBody(cpr::Body(json));
  const cpr::Response response = session_.Post();
  if (response.error) {
    ASSERT(false, "Failed to post to the server: {}!", response.error.message);
    return;
  }
}

void Server::PrintGameState() {
  INFO(
      "Game state:\nMap size: ({}, {})\nName: {}\nPoints: {}\nTurn: {}\nTick remain ms: {}\nRevive timeout: {} seconds",
      game_state_.map_size.x, game_state_.map_size.y, game_state_.name, game_state_.points, game_state_.turn,
      game_state_.tick_remain_ms, game_state_.revive_timeout_sec);
}

}  // namespace app