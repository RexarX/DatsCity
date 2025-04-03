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

void Application::UpdateLoop() {
  float last_update_time = 0.0f;
  server_tick_limit_sec_ = server_tick_rate_ == 0.0f ? 0.0f : 1.0f / server_tick_rate_;

  float current_time = 0.0f;
  utils::Timer timer;

  while (running_) {
    current_time = static_cast<float>(timer.GetElapsedSec());
    update_delta_time_ = current_time - last_update_time;
    if (server_tick_rate_ == 0 || update_delta_time_.GetSeconds() >= server_tick_limit_sec_) {
      // CORE_TRACE("Update loop: {}", update_delta_time_.GetMilliseconds());
      server_.Update();
      if (server_.GetState() == Server::State::Connected) {
        const GameState& game_state = server_.GetGameState();
        if (game_state.tick_remain_ms < server_tick_limit_sec_ * 1000 / 2) {
          WARN("Sleeping for {} ms!", game_state.tick_remain_ms);
          std::this_thread::sleep_for(std::chrono::milliseconds(game_state.tick_remain_ms));
          continue;
        }

        game_.Update(game_state);
        server_.PrintGameState();
      }

      last_update_time = current_time;
    }
  }

  running_ = false;
}

void Application::RenderLoop() {
  renderer_.Init(name_, window_width_, window_height_);

  float last_render_time_ = 0.0f;
  float framerate_limit_sec_ = framerate_limit_ == 0.0f ? 0.0f : 1.0f / framerate_limit_;
  GameState local_state;

  float current_time = 0.0f;
  utils::Timer timer;

  while (!renderer_.ShouldStop()) {
    current_time = static_cast<float>(timer.GetElapsedSec());
    render_delta_time_ = current_time - last_render_time_;

    if (framerate_limit_ == 0 || render_delta_time_.GetSeconds() >= framerate_limit_sec_) {
      // TRACE("Render loop: {}", render_delta_time_.GetMilliseconds());
      {
        std::lock_guard<std::mutex> lock(server_mutex_);
        local_state = server_.GetGameState();
      }
      renderer_.Update(render_delta_time_);
      renderer_.Render(render_delta_time_, local_state);

      last_render_time_ = current_time;
      ++frame_counter_;
    }
  }

  running_ = false;
}

}  // namespace app