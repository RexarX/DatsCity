#pragma once

#include "Game/GameObjects.h"

#include <raylib.h>

namespace app {

struct LetterCube {
  Texture2D texture;
  Vector3 position{0.0f, 0.0f, 0.0f};
  Direction direction = Direction::kUp;
  std::string letter;
  Color color{0, 0, 0};
  float scale = 0.0f;
  float rotation = 0.0f;
};

class Renderer {
public:
  Renderer() noexcept = default;
  Renderer(const Renderer&) = delete;
  Renderer(Renderer&&) noexcept = default;
  ~Renderer() {
    UnloadSkybox();
    UnloadModel(cube_model_);
    UnloadWordTextures();
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

  void DrawWordBlocks(const GameState& game_state);
  void DrawMapGrid(const Coords& map_size);

  void GenerateLetterTextures();
  void UnloadWordTextures();

  void DrawHUD(Timestep delta_time, const GameState& game_state);

  void DrawCubeTexture(const Texture2D& texture, const Vector3& position, float width, float height, float length,
                       Color color);

  void DrawText3D(std::string_view text, const Vector3& position, float font_size, float font_spacing, Color color);

  void DrawBillboardText(std::string_view text, const Vector3& position, float font_size, Color color);

  void SaveCubemapImage(const char* right_path, const char* left_path, const char* top_path, const char* bottom_path,
                        const char* front_path, const char* back_path, const char* output_path);

  static Image CreateCubemapImage(const char* right_path, const char* left_path, const char* top_path,
                                  const char* bottom_path, const char* front_path, const char* back_path);

  Texture2D GetOrCreateLetterTexture(std::string_view letter);

  Vector3 MeasureText3D(const Font& font, std::string_view text, float font_size, float font_spacing,
                        float line_spacing);

  void CreateTestLetterCubes();

private:
  Camera3D camera_{};
  float camera_angle_y_ = 0.0f;
  float camera_angle_x_ = 0.0f;
  float mouse_speed_ = 0.1f;
  float move_speed_ = 50.0f;

  bool show_grid_ = false;

  std::unordered_map<std::string_view, Texture2D> letter_textures_;
  std::vector<LetterCube> word_blocks_;
  std::vector<LetterCube> test_blocks_;
  float block_size_ = 10.0f;
  Color active_word_color_{0, 120, 255, 255};     // Blue for active tower
  Color completed_word_color_{0, 200, 100, 255};  // Green for completed towers

  // Grid visualization properties
  Color grid_color_{100, 100, 100, 50};

  Mesh skybox_mesh_;
  Model skybox_model_;
  Mesh cube_mesh_;
  Model cube_model_;
};

}  // namespace app