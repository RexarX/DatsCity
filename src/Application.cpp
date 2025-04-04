#include "Application.h"

namespace app {

Application::Application(std::string_view name, int window_width, int window_height, int framerate_limit,
                         int server_tick_rate) noexcept {
  if (instance_) {
    ASSERT(false, "Application already exists!");
    return;
  }

  instance_ = this;
  name_ = name;
  window_width_ = window_width;
  window_height_ = window_height;
  framerate_limit_ = framerate_limit;
  server_tick_rate_ = server_tick_rate;
}

void Application::Run() {
  update_thread_ = std::thread(&Application::UpdateLoop, this);
  render_thread_ = std::thread(&Application::RenderLoop, this);

  render_thread_.join();
  update_thread_.join();
}

void Application::Shutdown() {}

void Application::FetchInitialGameData() {
  std::lock_guard<std::mutex> lock(server_mutex_);

  bool success = true;
  
  if (const auto words_result = server_.FetchWords(); !words_result) {
    ERROR("Failed to fetch initial words: {}", words_result.error().error);
    success = false;
  } else {
    INFO("Fetched {} words for the current turn", words_result->words.size());
  }
 
  if (const auto towers_result = server_.FetchTowers(); !towers_result) {
    ERROR("Failed to fetch initial towers: {}", towers_result.error().error);
    success = false;
  } else {
    INFO("Fetched tower data, current score: {}", towers_result->score);
  }
  
  if (const auto rounds_result = server_.FetchRounds(); !rounds_result) {
    ERROR("Failed to fetch initial rounds: {}", rounds_result.error().error);
    success = false;
  } else {
    INFO("Fetched {} rounds", rounds_result->rounds.size());
  }

  if (success) {
    INFO("Initial game data fetched successfully");
  }
}

bool Application::CheckAndFetchWordData() {
  ShuffleResponse result;
  bool success = false;

  {
    std::lock_guard<std::mutex> lock(server_mutex_);
    const auto words_result = server_.FetchWords();
    success = words_result.has_value();
  }

  if (success) {
    state_needs_swap_.store(true, std::memory_order_release);
    INFO("Fetched words for the current turn");
  } else {
    ERROR("Failed to fetch words");
  }

  return success;
}

bool Application::CheckAndFetchTowerData() {
  bool success = false;

  {
    std::lock_guard<std::mutex> lock(server_mutex_);
    const auto towers_result = server_.FetchTowers();
    success = towers_result.has_value();
  }

  if (success) {
    state_needs_swap_.store(true, std::memory_order_release);
    INFO("Fetched tower data");
  } else {
    ERROR("Failed to fetch towers");
  }

  return success;
}

bool Application::ShuffleCurrentWords() {
  ShuffleResponse result;
  bool success = false;
  int shuffles_left = 0;

  {
    std::lock_guard<std::mutex> lock(server_mutex_);
    if (const auto shuffle_result = server_.ShuffleWords(); shuffle_result) {
      result = *shuffle_result;
      shuffles_left = result.shuffle_left;
      success = true;
    }
  }

  if (success) {
    state_needs_swap_.store(true, std::memory_order_release);
    INFO("Shuffled words, shuffles left: {}", shuffles_left);
  } else {
    ERROR("Failed to shuffle words");
  }

  return success;
}

bool Application::FetchGameRounds() {
  bool success = false;

  {
    std::lock_guard<std::mutex> lock(server_mutex_);
    const auto rounds_result = server_.FetchRounds();
    success = rounds_result.has_value();
  }

  if (success) {
    state_needs_swap_.store(true, std::memory_order_release);
    INFO("Fetched rounds");
  } else {
    ERROR("Failed to fetch rounds");
  }

  return success;
}

bool Application::BuildWordTower(const BuildRequest& request) {
  ShuffleResponse result;
  bool success = false;
  int shuffles_left = 0;

  {
    std::lock_guard<std::mutex> lock(server_mutex_);
    if (auto build_result = server_.BuildTower(request); build_result) {
      result = std::move(*build_result);
      shuffles_left = result.shuffle_left;
      success = true;
    }
  }

  if (success) {
    state_needs_swap_.store(true, std::memory_order_release);
    INFO("Tower built successfully, shuffles left: {}", shuffles_left);

    // Refresh tower data after building
    CheckAndFetchTowerData();
  } else {
    ERROR("Failed to build tower");
  }

  return success;
}

void Application::UpdateLoop() {
  float last_update_time = 0.0f;
  server_tick_limit_sec_ = server_tick_rate_ == 0.0f ? 0.0f : 1.0f / server_tick_rate_;

  float current_time = 0.0f;
  utils::Timer timer;

  bool initial_data_fetched = false;

  while (running_.load(std::memory_order_acquire)) {
    current_time = static_cast<float>(timer.GetElapsedSec());
    update_delta_time_ = current_time - last_update_time;

    if (server_tick_rate_ == 0 || update_delta_time_.GetSeconds() >= server_tick_limit_sec_) {
      // Update server state
      {
        std::lock_guard<std::mutex> lock(server_mutex_);
        server_.Update();
      }

      if (server_.GetState() == Server::State::kConnected) {
        // Fetch initial data if not already done
        if (!initial_data_fetched) {
          FetchInitialGameData();
          initial_data_fetched = true;
        }

        update_game_state_ = GetGameStateCopy();

        // Signal that state has changed
        state_needs_swap_.store(true, std::memory_order_release);

        // Sleep if needed
        /*if (update_game_state_.tick_remain_ms < server_tick_limit_sec_ * 1000 / 2) {
          WARN("Sleeping for {} ms!", update_game_state_.tick_remain_ms);
          std::this_thread::sleep_for(std::chrono::milliseconds(update_game_state_.tick_remain_ms));
          continue;
        }*/

        // Check if we have words data, if not, fetch it
        if (update_game_state_.words_data.words.empty()) {
          CheckAndFetchWordData();
        }

        // If we don't have tower data, fetch it too
        if (update_game_state_.towers_data.done_towers.empty() && !update_game_state_.towers_data.tower.has_value()) {
          CheckAndFetchTowerData();
        }

        // Update game logic
        game_.Update(update_game_state_);

        // Print game state info occasionally (not every frame to reduce console spam)
        if (update_game_state_.words_data.turn % 10 == 0) {
          std::lock_guard<std::mutex> lock(server_mutex_);
          server_.PrintGameState();
        }
      }

      last_update_time = current_time;
    }
  }
}

void Application::RenderLoop() {
  renderer_.Init(name_, window_width_, window_height_);

  float last_render_time_ = 0.0f;
  float framerate_limit_sec_ = framerate_limit_ == 0.0f ? 0.0f : 1.0f / framerate_limit_;

  float current_time = 0.0f;
  utils::Timer timer;

  while (!renderer_.ShouldStop()) {
    current_time = static_cast<float>(timer.GetElapsedSec());
    render_delta_time_ = current_time - last_render_time_;

    if (framerate_limit_ == 0 || render_delta_time_.GetSeconds() >= framerate_limit_sec_) {
      // Check if we need to swap game states
      if (state_needs_swap_.load(std::memory_order_acquire)) {
        SwapGameStates();
      }

      renderer_.Update(render_delta_time_);
      renderer_.Render(render_delta_time_, render_game_state_);

      last_render_time_ = current_time;
      ++frame_counter_;
    }

    // Small sleep to prevent render thread from consuming 100% CPU when frame limited
    /*if (framerate_limit_ > 0) {
      float time_to_next_frame = framerate_limit_sec_ - (static_cast<float>(timer.GetElapsedSec()) - last_render_time_);
      if (time_to_next_frame > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(time_to_next_frame * 500)));
      }
    }*/
  }

  running_.store(false, std::memory_order_release);
}

}  // namespace app