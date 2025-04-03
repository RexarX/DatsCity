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
  void SendJsonToServer(std::string_view json) { server_.Send(json); }

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

private:
  static inline Application* instance_ = nullptr;

  std::string name_;
  bool running_ = false;

  int window_width_ = 0;
  int window_height_ = 0;

  Game game_;
  Renderer renderer_;
  Server server_;

  std::mutex server_mutex_;

  std::thread update_thread_;
  std::thread render_thread_;

  Timestep render_delta_time_;
  int framerate_limit_ = 0;
  float framerate_limit_sec_ = 0.0f;
  int64_t frame_counter_ = 0;

  Timestep update_delta_time_;
  int server_tick_rate_ = 0;
  float server_tick_limit_sec_ = 0.0f;

  double current_time_ = 0.0;
};

inline Application* CreateApplication();

}  // namespace app