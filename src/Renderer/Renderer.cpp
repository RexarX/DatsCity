#include "Renderer.h"

#include <raymath.h>
#include <rlgl.h>

namespace app {

void Renderer::Init(std::string_view window_name, int width, int height) {
  InitWindow(width, height, window_name.data());
  SetWindowState(FLAG_WINDOW_RESIZABLE);
  SetWindowState(FLAG_VSYNC_HINT);
  ;

  camera_.position = Vector3{50.0f, 100.0f, 0.0f};
  camera_.target = Vector3{0.0f, 0.0f, 0.0f};
  camera_.up = Vector3{0.0f, 1.0f, 0.0f};
  camera_.fovy = 80.0f;
  camera_.projection = CAMERA_PERSPECTIVE;

  DisableCursor();
}

void Renderer::Update(Timestep delta_time) {
  UpdateCamera(delta_time);
}

void Renderer::Render(Timestep delta_time, const GameState& game_state) {
  BeginDrawing();
  ClearBackground(BLACK);

  BeginMode3D(camera_);

  rlEnableSmoothLines();

  EndMode3D();

  EndDrawing();
}

void Renderer::UpdateCamera(Timestep delta_time) {
  Vector2 mouse_delta_ = GetMouseDelta();

  camera_angle_y_ += (mouse_delta_.x * -mouse_speed_ * DEG2RAD);
  camera_angle_x_ += (mouse_delta_.y * -mouse_speed_ * DEG2RAD);

  camera_angle_x_ = Clamp(camera_angle_x_, -PI / 2.0f + 0.1f, PI / 2.0f - 0.1f);

  Vector3 forward = Vector3Normalize(Vector3Subtract(camera_.target, camera_.position));
  Vector3 right = Vector3CrossProduct(forward, camera_.up);

  if (IsKeyDown(KEY_W)) {
    camera_.position = Vector3Add(camera_.position, Vector3Scale(forward, move_speed_ * delta_time));
  }
  if (IsKeyDown(KEY_S)) {
    camera_.position = Vector3Subtract(camera_.position, Vector3Scale(forward, move_speed_ * delta_time));
  }
  if (IsKeyDown(KEY_A)) {
    camera_.position = Vector3Subtract(camera_.position, Vector3Scale(right, move_speed_ * delta_time));
  }
  if (IsKeyDown(KEY_D)) {
    camera_.position = Vector3Add(camera_.position, Vector3Scale(right, move_speed_ * delta_time));
  }

  float wheel_move_ = GetMouseWheelMove();
  camera_.fovy = Clamp(camera_.fovy - wheel_move_ * 5.0f, 20.0f, 120.0f);

  float camera_distance = Vector3Length(Vector3Subtract(camera_.target, camera_.position));
  Vector3 new_forward_ = {std::cos(camera_angle_x_) * std::sin(camera_angle_y_), std::sin(camera_angle_x_),
                          std::cos(camera_angle_x_) * std::cos(camera_angle_y_)};

  camera_.target = Vector3Add(camera_.position, Vector3Scale(new_forward_, camera_distance));
}

}  // namespace app