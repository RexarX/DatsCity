#include "Renderer.h"

#include <raymath.h>
#include <rlgl.h>

#if defined(PLATFORM_DESKTOP)
const int GLSL_VERSION = 330;
#else
const int GLSL_VERSION = 100;
#endif

namespace app {

static void CustomLog(int msg_type, const char* text, va_list args) {
  const int len = std::vsnprintf(nullptr, 0, text, args) + 1;
  if (len == 0) {
    return;
  }

  std::string formatted_text(len, '\0');
  std::vsnprintf(formatted_text.data(), len, text, args);

  // Remove null terminator from the string length
  formatted_text.pop_back();

  switch (msg_type) {
    case LOG_INFO:
      INFO(formatted_text);
      return;
    case LOG_ERROR:
      ERROR(formatted_text);
      return;
    case LOG_WARNING:
      WARN(formatted_text);
      return;
    case LOG_DEBUG:
      INFO(formatted_text);
      return;
    default:
      return;
  }
}

void Renderer::Init(std::string_view window_name, int width, int height) {
  SetTraceLogCallback(CustomLog);

  InitWindow(width, height, window_name.data());
  SetWindowState(FLAG_WINDOW_RESIZABLE);
  SetWindowState(FLAG_VSYNC_HINT);

  camera_.position = Vector3{50.0f, 100.0f, 0.0f};
  camera_.target = Vector3{0.0f, 0.0f, 0.0f};
  camera_.up = Vector3{0.0f, 1.0f, 0.0f};
  camera_.fovy = 80.0f;
  camera_.projection = CAMERA_PERSPECTIVE;

  LoadSkybox();

  DisableCursor();
}

void Renderer::Update(Timestep delta_time) {
  if (IsKeyPressed(KEY_F2)) {
    show_grid_ = !show_grid_;
  }

  UpdateCamera(delta_time);
}

void Renderer::Render(Timestep delta_time, const GameState& game_state) {
  BeginDrawing();
  ClearBackground(BLACK);

  BeginMode3D(camera_);

  rlEnableSmoothLines();

  DrawSkybox();

  if (show_grid_) {
    DrawGrid(100, 10);
  }

  EndMode3D();

  DrawHUD(delta_time, game_state);

  EndDrawing();
}

void Renderer::UpdateCamera(Timestep delta_time) {
  Vector2 mouse_delta_ = GetMouseDelta();

  camera_angle_y_ += (mouse_delta_.x * -mouse_speed_ * DEG2RAD);
  camera_angle_x_ += (mouse_delta_.y * -mouse_speed_ * DEG2RAD);

  camera_angle_x_ = Clamp(camera_angle_x_, -PI / 2.0f + 0.1f, PI / 2.0f - 0.1f);

  Vector3 forward = Vector3Normalize(Vector3Subtract(camera_.target, camera_.position));
  Vector3 right = Vector3CrossProduct(forward, camera_.up);

  float speed_multiplier = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL) ? 3.0f : 1.0f;
  float current_speed = move_speed_ * speed_multiplier;

  if (IsKeyDown(KEY_W)) {
    camera_.position = Vector3Add(camera_.position, Vector3Scale(forward, current_speed * delta_time));
  }
  if (IsKeyDown(KEY_S)) {
    camera_.position = Vector3Subtract(camera_.position, Vector3Scale(forward, current_speed * delta_time));
  }
  if (IsKeyDown(KEY_A)) {
    camera_.position = Vector3Subtract(camera_.position, Vector3Scale(right, current_speed * delta_time));
  }
  if (IsKeyDown(KEY_D)) {
    camera_.position = Vector3Add(camera_.position, Vector3Scale(right, current_speed * delta_time));
  }

  float wheel_move_ = GetMouseWheelMove();
  camera_.fovy = Clamp(camera_.fovy - wheel_move_ * 5.0f, 20.0f, 120.0f);

  float camera_distance = Vector3Length(Vector3Subtract(camera_.target, camera_.position));
  Vector3 new_forward = {std::cos(camera_angle_x_) * std::sin(camera_angle_y_), std::sin(camera_angle_x_),
                         std::cos(camera_angle_x_) * std::cos(camera_angle_y_)};

  camera_.target = Vector3Add(camera_.position, Vector3Scale(new_forward, camera_distance));
}

void Renderer::DrawSkybox() {
  if (!skybox_model_.meshes || skybox_model_.materials[0].maps[MATERIAL_MAP_CUBEMAP].texture.id == 0) {
    return;
  }

  rlDrawRenderBatchActive();
  rlDisableBackfaceCulling();
  rlDisableDepthMask();

  rlDisableBackfaceCulling();
  rlDisableDepthMask();

  Matrix mat_view = GetCameraMatrix(camera_);
  mat_view.m12 = 0;
  mat_view.m13 = 0;
  mat_view.m14 = 0;

  rlPushMatrix();
  DrawModel(skybox_model_, Vector3Zero(), 1.0f, WHITE);
  rlPopMatrix();

  rlEnableBackfaceCulling();
  rlEnableDepthMask();
}

void Renderer::LoadSkybox() {
  Image cubemap = CreateCubemapImage("Assets/Textures/Skybox/right.png", "Assets/Textures/Skybox/left.png",
                                     "Assets/Textures/Skybox/top.png", "Assets/Textures/Skybox/bottom.png",
                                     "Assets/Textures/Skybox/front.png", "Assets/Textures/Skybox/back.png");

  if (!cubemap.data) {
    ASSERT(false, "Failed to load skybox: Failed to create cubemap image!");
    return;
  }

  skybox_mesh_ = GenMeshCube(1.0f, 1.0f, 1.0f);
  skybox_model_ = LoadModelFromMesh(skybox_mesh_);

  skybox_model_.materials[0].shader = LoadShader(TextFormat("Assets/Shaders/Skybox/skybox.vert", GLSL_VERSION),
                                                 TextFormat("Assets/Shaders/Skybox/skybox.frag", GLSL_VERSION));

  if (skybox_model_.materials[0].shader.id == 0) {
    ASSERT(false, "Failed to load skybox shader!");
    UnloadImage(cubemap);
    UnloadModel(skybox_model_);
    return;
  }

  int environment_map_loc = GetShaderLocation(skybox_model_.materials[0].shader, "environmentMap");
  if (environment_map_loc == -1) {
    ASSERT(false, "Failed to find 'environmentMap' uniform in skybox shader!");
    UnloadImage(cubemap);
    UnloadShader(skybox_model_.materials[0].shader);
    UnloadModel(skybox_model_);
    return;
  }

  int material_map_cubemap = MATERIAL_MAP_CUBEMAP;
  SetShaderValue(skybox_model_.materials[0].shader,
                 GetShaderLocation(skybox_model_.materials[0].shader, "environmentMap"), &material_map_cubemap,
                 SHADER_UNIFORM_INT);

  skybox_model_.materials[0].maps[MATERIAL_MAP_CUBEMAP].texture =
      LoadTextureCubemap(cubemap, CUBEMAP_LAYOUT_AUTO_DETECT);

  if (skybox_model_.materials[0].maps[MATERIAL_MAP_CUBEMAP].texture.id == 0) {
    ASSERT(false, "Failed to load cubemap texture!");
    UnloadImage(cubemap);
    UnloadShader(skybox_model_.materials[0].shader);
    UnloadModel(skybox_model_);
    return;
  }

  UnloadImage(cubemap);
}

void Renderer::UnloadSkybox() {
  if (skybox_model_.materials) {
    if (skybox_model_.materials[0].maps) {
      if (skybox_model_.materials[0].maps[MATERIAL_MAP_CUBEMAP].texture.id) {
        UnloadTexture(skybox_model_.materials[0].maps[MATERIAL_MAP_CUBEMAP].texture);
      }
    }

    if (skybox_model_.materials[0].shader.id) {
      UnloadShader(skybox_model_.materials[0].shader);
    }
  }

  if (skybox_model_.meshes) {
    UnloadModel(skybox_model_);
  }

  skybox_model_ = {};
}

void Renderer::DrawHUD(Timestep delta_time, const GameState& game_state) {
  static char buffer[128];

  std::snprintf(buffer, sizeof(buffer), "FPS: %d", static_cast<uint32_t>(delta_time.GetFramerate()));

  DrawText(buffer, 10, 10, 20, WHITE);
  DrawText("ESC - Exit | F2 - Show/Hide grid\nWASD - Move | Ctrl - Speed boost | Mouse - Rotate | Scroll - FOV", 10,
           GetScreenHeight() - 50, 20, WHITE);
}

void Renderer::SaveCubemapImage(const char* right_path, const char* left_path, const char* top_path,
                                const char* bottom_path, const char* front_path, const char* back_path,
                                const char* output_path) {
  Image cubemap = CreateCubemapImage(right_path, left_path, top_path, bottom_path, front_path, back_path);
  if (cubemap.data) {
    ExportImage(cubemap, output_path);
    INFO("Cubemap saved successfully to: {}.", output_path);
    UnloadImage(cubemap);
  }
}

Image Renderer::CreateCubemapImage(const char* right_path, const char* left_path, const char* top_path,
                                   const char* bottom_path, const char* front_path, const char* back_path) {
  Image right = LoadImage(right_path);
  Image left = LoadImage(left_path);
  Image top = LoadImage(top_path);
  Image bottom = LoadImage(bottom_path);
  Image front = LoadImage(front_path);
  Image back = LoadImage(back_path);

  // Verify all images were loaded
  if (!right.data && !left.data && !top.data && !bottom.data && !front.data && !back.data) {
    ERROR("Failed to load one or more cubemap faces!");
    return Image{nullptr};
  }

  // Verify all images are square and the same size
  if (right.width != right.height || right.width != left.width || right.width != top.width ||
      right.width != bottom.width || right.width != front.width || right.width != back.width) {
    ImageResize(&right, 1024, 1024);
    ImageResize(&left, 1024, 1024);
    ImageResize(&top, 1024, 1024);
    ImageResize(&bottom, 1024, 1024);
    ImageResize(&front, 1024, 1024);
    ImageResize(&back, 1024, 1024);
  }

  int face_size = right.width;

  // Create image in cross layout (vertical cross)
  // Layout:
  //       [T]
  //    [L][F][R][B]
  //       [B]
  Image cubemap = GenImageColor(face_size * 4, face_size * 3, BLANK);

  ImageDraw(&cubemap, top, Rectangle{0, 0, static_cast<float>(face_size), static_cast<float>(face_size)},
            Rectangle{static_cast<float>(face_size), 0, static_cast<float>(face_size), static_cast<float>(face_size)},
            WHITE);

  ImageDraw(&cubemap, left, Rectangle{0, 0, static_cast<float>(face_size), static_cast<float>(face_size)},
            Rectangle{0, static_cast<float>(face_size), static_cast<float>(face_size), static_cast<float>(face_size)},
            WHITE);

  ImageDraw(&cubemap, front, Rectangle{0, 0, static_cast<float>(face_size), static_cast<float>(face_size)},
            Rectangle{static_cast<float>(face_size), static_cast<float>(face_size), static_cast<float>(face_size),
                      static_cast<float>(face_size)},
            WHITE);

  ImageDraw(&cubemap, right, Rectangle{0, 0, static_cast<float>(face_size), static_cast<float>(face_size)},
            Rectangle{static_cast<float>(face_size * 2), static_cast<float>(face_size), static_cast<float>(face_size),
                      static_cast<float>(face_size)},
            WHITE);

  ImageDraw(&cubemap, back, Rectangle{0, 0, static_cast<float>(face_size), static_cast<float>(face_size)},
            Rectangle{static_cast<float>(face_size * 3), static_cast<float>(face_size), static_cast<float>(face_size),
                      static_cast<float>(face_size)},
            WHITE);

  ImageDraw(&cubemap, bottom, Rectangle{0, 0, static_cast<float>(face_size), static_cast<float>(face_size)},
            Rectangle{static_cast<float>(face_size), static_cast<float>(face_size * 2), static_cast<float>(face_size),
                      static_cast<float>(face_size)},
            WHITE);

  UnloadImage(right);
  UnloadImage(left);
  UnloadImage(top);
  UnloadImage(bottom);
  UnloadImage(front);
  UnloadImage(back);

  return cubemap;
}

}  // namespace app