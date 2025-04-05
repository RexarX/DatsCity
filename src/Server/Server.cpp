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
        SERVER_INFO("Game has started! Connected successfully.");

        // Also fetch towers and rounds to complete the game state
        auto towers_result = FetchTowers();
        if (towers_result) {
          game_state_.towers_data = std::move(*towers_result);
        }

        auto rounds_result = FetchRounds();
        if (rounds_result) {
          game_state_.rounds_data = std::move(rounds_result->rounds);
        }

        // Print the complete game state after we've fetched all data
        PrintGameState();
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
    SERVER_INFO("Connected to game. Map size: ({}, {}, {})", game_state_.words_data.map_size.x,
                game_state_.words_data.map_size.y, game_state_.words_data.map_size.z);

    // Also fetch towers and rounds data for a more complete game state
    auto towers_result = FetchTowers();
    if (towers_result) {
      game_state_.towers_data = std::move(*towers_result);
    }

    auto rounds_result = FetchRounds();
    if (rounds_result) {
      game_state_.rounds_data = std::move(rounds_result->rounds);
    }

    PrintGameState();

    return;
  }

  const std::string& error_message = words_result.error().error;
  if (error_message.find("there is no active game") != std::string::npos || words_result.error().err_code == 23) {
    state_ = State::kWaitingForNextGame;
    ParseNoActiveGameError(error_message);
  } else {
    SERVER_ERROR("Error while updating server: {}!", error_message);
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

  SERVER_INFO("Shuffling words");

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
    SERVER_INFO("Game state: No words data available");
    return;
  }

  // Count words in the current tower (if it exists)
  int words_in_tower = 0;
  double tower_score = 0.0;
  if (game_state_.towers_data.tower.has_value()) {
    words_in_tower = static_cast<int>(game_state_.towers_data.tower->words.size());
    tower_score = game_state_.towers_data.tower->score;
  }

  // Find the active round (status is "active" or not "finished")
  std::string_view round_status = "Unknown";
  std::string_view round_name = "None";
  std::string_view round_ends_at = game_state_.words_data.round_ends_at.c_str();

  bool found_active_round = false;

  for (const auto& round : game_state_.rounds_data) {
    // Check if this round is active
    if (round.status == "active" || (round.status != "finished" && !found_active_round)) {
      round_status = round.status.c_str();
      round_name = round.name.c_str();

      // If we found a truly active round, break immediately
      if (round.status == "active") {
        found_active_round = true;
        break;
      }
    }
  }

  // If there's no active round but we have rounds data, show the most recent round
  if (!found_active_round && !game_state_.rounds_data.empty()) {
    // Sort rounds by end time (descending) to find the most recent
    auto sorted_rounds = game_state_.rounds_data;
    std::sort(sorted_rounds.begin(), sorted_rounds.end(),
              [](const Round& a, const Round& b) { return a.end_at > b.end_at; });

    // Use the most recent round for display
    round_status = sorted_rounds[0].status.c_str();
    round_name = sorted_rounds[0].name.c_str();
    SERVER_INFO("No active round found, showing most recent round");
  }

  SERVER_INFO(R"(
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
              game_state_.words_data.shuffle_left, round_name, round_status, round_ends_at,
              game_state_.towers_data.done_towers.size(), game_state_.towers_data.score,
              game_state_.towers_data.tower.has_value() ? "Active" : "None", words_in_tower, tower_score);

  // Display extra information about all rounds if there are multiple
  if (game_state_.rounds_data.size() > 1) {
    SERVER_INFO("All Rounds:");
    SERVER_INFO("+--------------------+------------+------------------------+------------------------+");
    SERVER_INFO("| Round Name         | Status     | Start Time             | End Time               |");
    SERVER_INFO("+--------------------+------------+------------------------+------------------------+");

    for (const auto& round : game_state_.rounds_data) {
      SERVER_INFO("| {:<18} | {:<10} | {:<22} | {:<22} |", round.name, round.status, round.start_at, round.end_at);
    }

    SERVER_INFO("+--------------------+------------+------------------------+------------------------+");
  }
}

std::expected<std::string, Server::Error> Server::SendGetRequest(std::string_view endpoint) const {
  if (state_ == State::kDisconnected) {
    return std::unexpected(Error{.error = "Server is not connected"});
  }

  SERVER_INFO("Sending GET request to {}", endpoint);

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
      Error api_error;
      const auto parse_err = glz::read_json(api_error, response.text);

      if (!parse_err) {
        // Successfully parsed structured error with next_rounds
        return std::unexpected(api_error);
      }

      // Try ErrorResponse format
      ErrorResponse simple_error;
      const auto simple_parse_err = glz::read_json(simple_error, response.text);

      if (!simple_parse_err) {
        return std::unexpected(Error{.error = simple_error.message, .err_code = simple_error.code});
      }

      // If standard JSON parsing fails, try manual extraction with regex
      const std::regex error_regex("\\\"error\\\":\\\"([^\\\"]+)\\\",\\\"errCode\\\":(\\d+)");
      std::smatch matches;

      if (std::regex_search(response.text, matches, error_regex) && matches.size() >= 3) {
        return std::unexpected(Error{.error = matches[1].str(), .err_code = std::stoi(matches[2].str())});
      }

      // If all parsing attempts fail, return the raw error text
      return std::unexpected(Error{.error = std::format("HTTP error {}: {}", response.status_code, response.text)});

    } catch (const std::exception& e) {
      return std::unexpected(Error{.error = std::format("Error parsing response: {}", e.what())});
    }
  }

  return response.text;
}

void Server::ParseNoActiveGameError(const std::string& error_message) {
  // Try to extract the next rounds directly from the error message using regex
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
      next_game_check_time_ = now;
      SERVER_INFO(
          "Game '{}' should be in progress! Start time was {} and end time is {}. Attempting to connect immediately.",
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

      SERVER_INFO("No active game. Next game '{}' starts in {} seconds (at {}).", round_name, time_until_game,
                  start_time_str);
    } else {
      // Both start and end times are in the past - game is over
      // Check for the next game in a minute
      next_game_check_time_ = now + std::chrono::minutes(1);
      SERVER_INFO("Game '{}' appears to have ended (ran from {} to {}). Will check for new games in 60 seconds.",
                  round_name, start_time_str, end_time_str);
    }
  } else {
    // Try to parse the structured error with next_rounds field
    try {
      Error parsed_error;
      const auto parse_err = glz::read_json(parsed_error, error_message);

      if (!parse_err && !parsed_error.next_rounds.empty()) {
        SERVER_INFO("Upcoming game rounds:");

        // Print a table of upcoming rounds
        SERVER_INFO("+--------------------+------------------------+------------------------+");
        SERVER_INFO("| Round Name         | Start Time             | End Time               |");
        SERVER_INFO("+--------------------+------------------------+------------------------+");

        const auto now = std::chrono::system_clock::now();
        bool found_next_game = false;

        for (const auto& round : parsed_error.next_rounds) {
          SERVER_INFO("| {:<18} | {:<22} | {:<22} |", round.name, round.start_time, round.end_time);

          // Parse the start time to determine when to check next
          if (!found_next_game) {
            std::tm start_tm = {};
            std::istringstream start_ss(round.start_time);
            start_ss >> std::get_time(&start_tm, "%Y-%m-%dT%H:%M:%SZ");

            if (!start_ss.fail()) {
              const auto start_time = std::chrono::system_clock::from_time_t(std::mktime(&start_tm));

              if (start_time > now) {
                found_next_game = true;

                // Set next check time based on the next upcoming game
                const auto time_until_game = std::chrono::duration_cast<std::chrono::seconds>(start_time - now).count();

                // Check 10 seconds before the game starts
                next_game_check_time_ = start_time - std::chrono::seconds(10);

                // If the game is starting in more than 10 minutes, check every minute
                if (time_until_game > 600) {
                  next_game_check_time_ = now + std::chrono::minutes(1);
                }

                SERVER_INFO("Will check for game '{}' in {} seconds.", round.name,
                            std::chrono::duration_cast<std::chrono::seconds>(next_game_check_time_ - now).count());
              }
            }
          }
        }
        SERVER_INFO("+--------------------+------------------------+------------------------+");

        // If we didn't find any future games, check again in 60 seconds
        if (!found_next_game) {
          next_game_check_time_ = now + std::chrono::minutes(1);
          SERVER_INFO("No upcoming games found. Will check again in 60 seconds.");
        }

        return;
      }
    } catch (const std::exception& e) {
      SERVER_WARN("Exception while parsing error JSON: {}.", e.what());
    }

    // If we can't parse the time or structured error, check again in 60 seconds
    next_game_check_time_ = std::chrono::system_clock::now() + std::chrono::seconds(60);
    SERVER_INFO("Could not parse next game time. Will check again in 60 seconds. Error message: {}.", error_message);
  }
}

}  // namespace app