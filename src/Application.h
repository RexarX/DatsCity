#pragma once

#include "Game/Game.h"
#include "Renderer/Renderer.h"
#include "Server/Server.h"

namespace app {

class Application {
public:
  Application(std::string_view name, int window_width = 1280, int window_height = 720, int framerate_limit = 0,
              int server_tick_rate = 1) noexcept;
  Application(const Application&) = delete;
  Application(Application&&) = delete;
  ~Application() { Shutdown(); }

  void Run();

  void ConnectToServer(std::string_view url, std::string_view token) { server_.Connect(url, token); }

  bool CheckAndFetchWordData();
  bool CheckAndFetchTowerData();
  bool ShuffleCurrentWords();
  bool FetchGameRounds();
  bool BuildWordTower(const BuildRequest& request);

  // Game state accessors
  inline const WordsResponse& GetCurrentWords() const noexcept { return render_game_state_.words_data; }
  inline const TowersResponse& GetCurrentTowers() const noexcept { return render_game_state_.towers_data; }
  inline const std::vector<Round>& GetGameRounds() const noexcept { return render_game_state_.rounds_data; }

  void SetFramerateLimit(int framerate_limit) noexcept {
    framerate_limit_ = framerate_limit;
    framerate_limit_sec_ = framerate_limit > 0.0f ? 1.0f / framerate_limit : 0.0f;
  }

  Application& operator=(const Application&) = delete;
  Application& operator=(Application&&) = delete;

  inline const std::string& GetName() const noexcept { return name_; }

  inline Timestep GetDeltaTime() const noexcept { return render_delta_time_; }
  inline int GetFramerateLimit() const noexcept { return framerate_limit_; }
  inline int64_t GetFrameCount() const noexcept { return frame_counter_; }

  inline int GetServerTickRate() const noexcept { return server_tick_rate_; }

  static inline Application& Get() noexcept { return *instance_; }

private:
  void Shutdown();

  void UpdateLoop();
  void RenderLoop();

  void FetchInitialGameData();

  inline GameState GetGameStateCopy() {
    std::lock_guard<std::mutex> lock(server_mutex_);
    return server_.GetGameState();
  }

  void SwapGameStates() {
    std::lock_guard<std::mutex> lock(state_swap_mutex_);
    render_game_state_ = update_game_state_;
    state_needs_swap_.store(false, std::memory_order_release);
  }

private:
  static inline Application* instance_ = nullptr;

  std::string name_;
  std::atomic<bool> running_{true};

  int window_width_ = 0;
  int window_height_ = 0;

  Game game_;
  Renderer renderer_;
  Server server_;

  // Double buffering for game state to reduce contention
  GameState update_game_state_;
  GameState render_game_state_;
  std::atomic<bool> state_needs_swap_{false};

  std::mutex server_mutex_;      // For server API calls
  std::mutex state_swap_mutex_;  // For swapping game states

  std::thread update_thread_;
  std::thread render_thread_;

  Timestep render_delta_time_;
  int framerate_limit_ = 0;
  float framerate_limit_sec_ = 0.0f;
  int64_t frame_counter_ = 0;

  Timestep update_delta_time_;
  int server_tick_rate_ = 0;
  float server_tick_limit_sec_ = 0.0f;
};

}  // namespace app