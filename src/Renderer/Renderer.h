#pragma once

#include "Game/GameObjects.h"

#include <raylib.h>

namespace app {

class Renderer {
public:
  constexpr Renderer() noexcept = default;
  Renderer(const Renderer&) = delete;
  constexpr Renderer(Renderer&&) noexcept = default;
  ~Renderer() {
    UnloadSkybox();
    CloseWindow();
  }

  void Init(std::string_view window_name, int width, int height);
  void Update(Timestep delta_time);
  void Render(Timestep delta_time, const GameState& game_state);

  Renderer& operator=(const Renderer&) = delete;
  inline Renderer& operator=(Renderer&&) noexcept = default;

  inline bool ShouldStop() const noexcept { return WindowShouldClose(); }

private:
  void UpdateCamera(Timestep delta_time);

  void DrawSkybox();
  void LoadSkybox();
  void UnloadSkybox();

  void DrawHUD(Timestep delta_time, const GameState& game_state);

  void SaveCubemapImage(const char* right_path, const char* left_path, const char* top_path, const char* bottom_path,
                        const char* front_path, const char* back_path, const char* output_path);

  static Image CreateCubemapImage(const char* right_path, const char* left_path, const char* top_path,
                                  const char* bottom_path, const char* front_path, const char* back_path);

private:
  Camera3D camera_{};
  float camera_angle_y_ = 0.0f;
  float camera_angle_x_ = 0.0f;
  float mouse_speed_ = 0.1f;
  float move_speed_ = 50.0f;

  bool show_grid_ = false;

  Mesh skybox_mesh_;
  Model skybox_model_;
};

}  // namespace app