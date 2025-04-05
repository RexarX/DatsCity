#include "Server.h"

namespace app {

namespace {

constexpr std::string_view kBuildEndpoint = "build";
constexpr std::string_view kShuffleEndpoint = "shuffle";
constexpr std::string_view kTowersEndpoint = "towers";
constexpr std::string_view kWordsEndpoint = "words";
constexpr std::string_view kRoundsEndpoint = "rounds";

}  // namespace

void Server::Connect(std::string_view url, std::string_view token) {
  if (url.empty()) {
    ASSERT(false, "Failed to connect to the server: url is empty!");
    return;
  }

  if (token.empty()) {
    ASSERT(false, "Failed to connect to the server: token is empty!");
    return;
  }

  if (url == base_url_ && token == token_ && state_ == State::kConnected) {
    return;
  }

  base_url_ = url;
  token_ = token;

  if (base_url_.back() != '/') {
    base_url_.push_back('/');
  }

  session_.SetUrl(base_url_);
  session_.SetHeader({{"X-Auth-Token", token_}, {"Content-Type", "application/json"}});

  state_ = State::kConnected;
}

void Server::Update() {
  if (state_ == State::kDisconnected) {
    ASSERT(false, "Failed to update server: Server is not connected!");
    return;
  }

  // If we're waiting for the next game, check if it's time to try connecting again
  if (state_ == State::kWaitingForNextGame) {
    const auto now = std::chrono::system_clock::now();
    if (now >= next_game_check_time_) {
      // Try to fetch words to see if the game has started
      auto words_result = FetchWords();
      if (words_result) {
        state_ = State::kConnected;
        game_state_.words_data = std::move(*words_result);
        INFO("Game has started! Connected successfully.");

        // Also fetch towers and rounds to complete the game state
        auto towers_result = FetchTowers();
        if (towers_result) {
          game_state_.towers_data = std::move(*towers_result);
        }

        auto rounds_result = FetchRounds();
        if (rounds_result) {
          game_state_.rounds_data = std::move(rounds_result->rounds);
        }

        return;
      } else {
        // Still no active game, update next check time
        ParseNoActiveGameError(words_result.error().error);
      }
    }
    return;
  }

  // Try a GET request to words endpoint instead of POSTing to base URL
  auto words_result = FetchWords();
  if (words_result) {
    state_ = State::kConnected;
    game_state_.words_data = std::move(*words_result);

    // Log the map size from the received words data
    INFO("Connected to game. Map size: ({},{},{})", game_state_.words_data.map_size.x,
         game_state_.words_data.map_size.y, game_state_.words_data.map_size.z);
    return;
  }

  const std::string& error_message = words_result.error().error;
  if (error_message.find("there is no active game") != std::string::npos || words_result.error().err_code == 23) {
    state_ = State::kWaitingForNextGame;
    ParseNoActiveGameError(error_message);
  } else {
    ERROR("Error while updating server: {}!", error_message);
  }
}

std::expected<TowersResponse, Server::Error> Server::FetchTowers() {
  const auto response = SendGetRequest(kTowersEndpoint);
  if (!response) {
    return std::unexpected(response.error());
  }

  TowersResponse towers;
  const auto err = glz::read_json(towers, *response);
  if (err) {
    return std::unexpected(
        Error{.error = std::format("Failed to parse towers response: {}", glz::format_error(err, *response))});
  }

  // Update cached data
  game_state_.towers_data = towers;

  return towers;
}

std::expected<WordsResponse, Server::Error> Server::FetchWords() {
  const auto response = SendGetRequest(kWordsEndpoint);
  if (!response) {
    return std::unexpected(response.error());
  }

  WordsResponse words;
  const auto err = glz::read_json(words, *response);
  if (err) {
    return std::unexpected(
        Error{.error = std::format("Failed to parse words response: {}", glz::format_error(err, *response))});
  }

  // Update cached data
  game_state_.words_data = words;

  return words;
}

std::expected<ShuffleResponse, Server::Error> Server::ShuffleWords() const {
  if (state_ == State::kDisconnected) {
    return std::unexpected(Error{.error = "Server is not connected"});
  }

  if (state_ == State::kWaitingForNextGame) {
    return std::unexpected(Error{.error = "No active game"});
  }

  INFO("Shuffling words");

  cpr::Session req;
  req.SetUrl(FormatEndpointUrl(kShuffleEndpoint));
  req.SetHeader({{"X-Auth-Token", token_}, {"Content-Type", "application/json"}});
  req.SetBody("{}");

  const cpr::Response response = req.Post();

  if (response.error) {
    return std::unexpected(Error{.error = std::format("Request failed: {}", response.error.message)});
  }

  if (response.status_code != 200) {
    try {
      ErrorResponse api_error;
      const auto parse_err = glz::read_json(api_error, response.text);
      if (parse_err) {
        return std::unexpected(Error{.error = std::format("HTTP error {}: {}", response.status_code, response.text)});
      }
      return std::unexpected(Error{.error = std::format("Error code {}: {}", api_error.code, api_error.message)});

    } catch (const std::exception& e) {
      return std::unexpected(Error{.error = std::format("Error parsing response: {}", e.what())});
    }
  }

  ShuffleResponse result;
  const auto err = glz::read_json(result, response.text);
  if (err) {
    return std::unexpected(
        Error{.error = std::format("Failed to parse shuffle response: {}", glz::format_error(err, response.text))});
  }

  return result;
}

std::expected<RoundsResponse, Server::Error> Server::FetchRounds() {
  const auto response = SendGetRequest(kRoundsEndpoint);
  if (!response) {
    return std::unexpected(response.error());
  }

  RoundsResponse rounds;
  const auto err = glz::read_json(rounds, *response);
  if (err) {
    return std::unexpected(
        Error{.error = std::format("Failed to parse rounds response: {}", glz::format_error(err, *response))});
  }

  // Update cached data
  game_state_.rounds_data = rounds.rounds;

  return rounds;
}

std::expected<ShuffleResponse, Server::Error> Server::BuildTower(const BuildRequest& request) {
  const auto response = SendRequest(kBuildEndpoint, request);
  if (!response) {
    return std::unexpected(response.error());
  }

  ShuffleResponse result;
  const auto err = glz::read_json(result, *response);
  if (err) {
    return std::unexpected(
        Error{.error = std::format("Failed to parse build response: {}", glz::format_error(err, *response))});
  }

  return result;
}

void Server::PrintGameState() {
  if (game_state_.words_data.words.empty()) {
    INFO("Game state: No words data available");
    return;
  }

  // Count words in the current tower (if it exists)
  int words_in_tower = 0;
  double tower_score = 0.0;
  if (game_state_.towers_data.tower.has_value()) {
    words_in_tower = static_cast<int>(game_state_.towers_data.tower->words.size());
    tower_score = game_state_.towers_data.tower->score;
  }

  // Retrieve current round status
  std::string_view round_status = "Unknown";
  std::string_view round_name = "None";
  if (!game_state_.rounds_data.empty()) {
    round_status = game_state_.rounds_data[0].status.c_str();
    round_name = game_state_.rounds_data[0].name.c_str();
  }

  INFO(R"(
Game State Summary
-----------------------------------------
Map: [{}, {}, {}] | Turn: {} | Next turn: {}s
Words: {} available | Shuffles left: {}
Round: {} ({}) | Ends at: {}
-----------------------------------------
Towers completed: {} | Total score: {:.5f}
Current tower: {} | Words placed: {} | Score: {:.5f}
-----------------------------------------)",
       game_state_.words_data.map_size.x, game_state_.words_data.map_size.y, game_state_.words_data.map_size.z,
       game_state_.words_data.turn, game_state_.words_data.next_turn_sec, game_state_.words_data.words.size(),
       game_state_.words_data.shuffle_left, round_name, round_status, game_state_.words_data.round_ends_at,
       game_state_.towers_data.done_towers.size(), game_state_.towers_data.score,
       game_state_.towers_data.tower.has_value() ? "Active" : "None", words_in_tower, tower_score);
}

std::expected<std::string, Server::Error> Server::SendGetRequest(std::string_view endpoint) const {
  if (state_ == State::kDisconnected) {
    return std::unexpected(Error{.error = "Server is not connected"});
  }

  INFO("Sending GET request to {}", endpoint);

  cpr::Session req;
  req.SetUrl(FormatEndpointUrl(endpoint));
  req.SetHeader({{"X-Auth-Token", token_}, {"Content-Type", "application/json"}});

  const cpr::Response response = req.Get();

  if (response.error) {
    return std::unexpected(Error{.error = std::format("Request failed: {}", response.error.message)});
  }

  if (response.status_code != 200) {
    try {
      // Try to parse as structured JSON first
      ErrorResponse api_error;
      const auto parse_err = glz::read_json(api_error, response.text);

      if (parse_err) {
        // If standard JSON parsing fails, try manual extraction with regex
        const std::regex error_regex("\\\"error\\\":\\\"([^\\\"]+)\\\",\\\"errCode\\\":(\\d+)");
        std::smatch matches;

        if (std::regex_search(response.text, matches, error_regex) && matches.size() >= 3) {
          return std::unexpected(Error{.error = matches[1].str(), .err_code = std::stoi(matches[2].str())});
        }

        // If all parsing attempts fail, return the raw error text
        return std::unexpected(Error{.error = std::format("HTTP error {}: {}", response.status_code, response.text)});
      }

      return std::unexpected(Error{.error = api_error.message, .err_code = api_error.code});

    } catch (const std::exception& e) {
      return std::unexpected(Error{.error = std::format("Error parsing response: {}", e.what())});
    }
  }

  return response.text;
}

void Server::ParseNoActiveGameError(const std::string& error_message) {
  const std::regex round_regex(
      R"(next rounds: \[([\w-]+) (\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z) -- (\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z)\])");
  std::smatch matches;

  if (std::regex_search(error_message, matches, round_regex) && matches.size() >= 4) {
    const std::string round_name = matches[1].str();
    const std::string start_time_str = matches[2].str();
    const std::string end_time_str = matches[3].str();

    // Convert ISO 8601 strings to time_points
    std::tm start_tm = {};
    std::istringstream start_ss(start_time_str);
    start_ss >> std::get_time(&start_tm, "%Y-%m-%dT%H:%M:%SZ");

    std::tm end_tm = {};
    std::istringstream end_ss(end_time_str);
    end_ss >> std::get_time(&end_tm, "%Y-%m-%dT%H:%M:%SZ");

    const auto start_time = std::chrono::system_clock::from_time_t(std::mktime(&start_tm));
    const auto end_time = std::chrono::system_clock::from_time_t(std::mktime(&end_tm));
    const auto now = std::chrono::system_clock::now();

    // Check if the game should be ongoing (started in past but ends in future)
    if (start_time <= now && now < end_time) {
      // This is the case you identified - game has started but we're not seeing it
      // Try connecting immediately and more frequently since the game should be in progress
      next_game_check_time_ = now;  // Check immediately
      INFO("Game '{}' should be in progress! Start time was {} and end time is {}. Attempting to connect immediately.",
           round_name, start_time_str, end_time_str);
    } else if (start_time > now) {
      // Future game case (handled correctly in the original code)
      const auto time_until_game = std::chrono::duration_cast<std::chrono::seconds>(start_time - now).count();

      // Set next check time to 10 seconds before the game starts
      next_game_check_time_ = start_time - std::chrono::seconds(10);

      // If the game is starting in more than 10 minutes, check every minute
      if (time_until_game > 600) {
        next_game_check_time_ = now + std::chrono::minutes(1);
      }

      INFO("No active game. Next game '{}' starts in {} seconds (at {})", round_name, time_until_game, start_time_str);
    } else {
      // Both start and end times are in the past - game is over
      // Check for the next game in a minute
      next_game_check_time_ = now + std::chrono::minutes(1);
      INFO("Game '{}' appears to have ended (ran from {} to {}). Will check for new games in 60 seconds.", round_name,
           start_time_str, end_time_str);
    }
  } else {
    // If we can't parse the time, check again in 60 seconds
    next_game_check_time_ = std::chrono::system_clock::now() + std::chrono::seconds(60);
    INFO("Could not parse next game time. Will check again in 60 seconds. Error message: {}.", error_message);
  }
}

}  // namespace app