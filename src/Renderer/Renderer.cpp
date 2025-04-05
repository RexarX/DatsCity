#include "Renderer.h"

#include <raymath.h>
#include <rlgl.h>

#if defined(PLATFORM_DESKTOP)
const int GLSL_VERSION = 330;
#else
const int GLSL_VERSION = 100;
#endif

const std::array<std::string, 33> russian_letters = {"А", "Б", "В", "Г", "Д", "Е", "Ё", "Ж", "З", "И", "Й",
                                                     "К", "Л", "М", "Н", "О", "П", "Р", "С", "Т", "У", "Ф",
                                                     "Х", "Ц", "Ч", "Ш", "Щ", "Ъ", "Ы", "Ь", "Э", "Ю", "Я"};

namespace app {

static void CustomLog(int msg_type, const char* text, va_list args) {
  const int len = std::vsnprintf(nullptr, 0, text, args) + 1;
  if (len == 0) {
    return;
  }

  std::string formatted_text(len, '\0');
  std::vsnprintf(formatted_text.data(), len, text, args);
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

  rlEnableDepthTest();
  rlSetBlendMode(RL_BLEND_ALPHA);
  SetConfigFlags(FLAG_MSAA_4X_HINT);
  rlSetCullFace(RL_CULL_FACE_BACK);

  LoadSkybox();

  cube_mesh_ = GenMeshCube(1.0f, 1.0f, 1.0f);
  cube_model_ = LoadModelFromMesh(cube_mesh_);

  GenerateLetterTextures();
  CreateTestLetterCubes();

  DisableCursor();

  universal_cube_color_ = {120, 120, 255, 255};
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
  DrawWordBlocks(game_state);

  for (const auto& block : test_blocks_) {
    DrawCubeTexture(block.texture, block.position, block_size_, block_size_, block_size_, universal_cube_color_);
  }

  if (show_grid_) {
    DrawMapGrid(game_state.words_data.map_size);
  }

  EndMode3D();

  DrawHUD(delta_time, game_state);
  EndDrawing();
}

void Renderer::UpdateCamera(Timestep delta_time) {
  const Vector2 mouse_delta_ = GetMouseDelta();

  camera_angle_y_ += (mouse_delta_.x * -mouse_speed_ * DEG2RAD);
  camera_angle_x_ += (mouse_delta_.y * -mouse_speed_ * DEG2RAD);

  camera_angle_x_ = Clamp(camera_angle_x_, -PI / 2.0f + 0.1f, PI / 2.0f - 0.1f);

  const Vector3 forward = Vector3Normalize(Vector3Subtract(camera_.target, camera_.position));
  const Vector3 right = Vector3CrossProduct(forward, camera_.up);

  const float speed_multiplier = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL) ? 3.0f : 1.0f;
  const float current_speed = move_speed_ * speed_multiplier;

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

  const float wheel_move_ = GetMouseWheelMove();
  camera_.fovy = Clamp(camera_.fovy - wheel_move_ * 5.0f, 20.0f, 120.0f);

  const float camera_distance = Vector3Length(Vector3Subtract(camera_.target, camera_.position));
  const Vector3 new_forward = {std::cos(camera_angle_x_) * std::sin(camera_angle_y_), std::sin(camera_angle_x_),
                               std::cos(camera_angle_x_) * std::cos(camera_angle_y_)};

  camera_.target = Vector3Add(camera_.position, Vector3Scale(new_forward, camera_distance));
}

void Renderer::DrawSkybox() {
  if (!skybox_model_.meshes || skybox_model_.materials[0].maps[MATERIAL_MAP_CUBEMAP].texture.id == 0) {
    return;
  }

  rlDrawRenderBatchActive();

  rlDisableDepthMask();
  rlDisableBackfaceCulling();

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

  const int environment_map_loc = GetShaderLocation(skybox_model_.materials[0].shader, "environmentMap");
  if (environment_map_loc == -1) {
    ASSERT(false, "Failed to find 'environmentMap' uniform in skybox shader!");
    UnloadImage(cubemap);
    UnloadShader(skybox_model_.materials[0].shader);
    UnloadModel(skybox_model_);
    return;
  }

  const int material_map_cubemap = MATERIAL_MAP_CUBEMAP;
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

void Renderer::DrawWordBlocks(const GameState& game_state) {
  word_blocks_.clear();

  if (game_state.towers_data.tower.has_value()) {
    const auto& tower = game_state.towers_data.tower.value();
    for (const auto& word : tower.words) {
      const Vector3 pos = {static_cast<float>(word.pos.x) * block_size_, static_cast<float>(word.pos.y) * block_size_,
                           static_cast<float>(word.pos.z) * block_size_};

      size_t i = 0;
      size_t byte_pos = 0;

      while (byte_pos < word.text.length()) {
        int char_size = 0;
        const uint8_t c = static_cast<uint8_t>(word.text[byte_pos]);

        if ((c & 0x80) == 0) {
          char_size = 1;
        } else if ((c & 0xE0) == 0xC0) {
          char_size = 2;
        } else if ((c & 0xF0) == 0xE0) {
          char_size = 3;
        } else if ((c & 0xF8) == 0xF0) {
          char_size = 4;
        } else {
          ++byte_pos;
          continue;
        }

        if (byte_pos + char_size > word.text.length()) {
          break;
        }

        const std::string letter = word.text.substr(byte_pos, char_size);
        const Coords offset = DirectionToCoords(word.dir);
        const Vector3 letter_pos = {pos.x + offset.x * block_size_ * static_cast<float>(i),
                                    pos.y + offset.y * block_size_ * static_cast<float>(i),
                                    pos.z + offset.z * block_size_ * static_cast<float>(i)};

        word_blocks_.push_back(
            {GetOrCreateLetterTexture(letter), letter_pos, word.dir, letter, universal_cube_color_, 1.0f, 0.0f});

        byte_pos += char_size;
        ++i;
      }
    }
  }

  for (const auto& block : word_blocks_) {
    DrawCubeTexture(block.texture, block.position, block_size_, block_size_, block_size_, universal_cube_color_);
  }
}

void Renderer::DrawMapGrid(const Coords& map_size) {
  const float width = block_size_ * map_size.x;
  const float height = block_size_ * map_size.y;
  const float depth = block_size_ * map_size.z;
  const float size = std::min({width, height, depth}) / 100;

  const Color x_axis_color = {255, 50, 50, 255};
  const Color y_axis_color = {50, 255, 50, 255};
  const Color z_axis_color = {50, 50, 255, 255};
  const Color grid_line_color = {180, 180, 180, 120};

  DrawLine3D({0, 0, 0}, {size + 10.0f, 0, 0}, x_axis_color);
  DrawLine3D({0, 0, 0}, {0, size + 10.0f, 0}, y_axis_color);
  DrawLine3D({0, 0, 0}, {0, 0, size + 10.0f}, z_axis_color);

  DrawText3D("X", {size + 12.0f, 0, 0}, 20.0f, 1.0f, x_axis_color);
  DrawText3D("Y", {0, size + 12.0f, 0}, 20.0f, 1.0f, y_axis_color);
  DrawText3D("Z", {0, 0, size + 12.0f}, 20.0f, 1.0f, z_axis_color);

  rlPushMatrix();
  rlRotatef(90.0f, 1.0f, 0.0f, 0.0f);

  constexpr std::string_view title = "DatsCity";
  const Vector3 text_size = MeasureText3D(GetFontDefault(), title.data(), 30.0f, 2.0f, 0.0f);
  const Vector3 text_pos = {width / 2.0f - text_size.x / 2.0f, 5.0f, depth / 2.0f - text_size.z / 2.0f};

  DrawText3D(title, text_pos, 30.0f, 2.0f, RED);
  rlPopMatrix();

  DrawCubeWiresV({width / 2, height / 2, depth / 2}, {width, height, depth}, grid_line_color);
}

void Renderer::GenerateLetterTextures() {
  constexpr int texture_size = 128;
  constexpr int font_size = 96;
  constexpr const char* font_path = "Assets/Fonts/Roboto-Bold.ttf";

  int charsCount = 0;
  int* chars = LoadCodepoints(
      "АБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯабвгдеёжзийклмнопрстуфхцчшщъыьэюя"
      "0123456789"
      ".,!?-+()[]{}:;/\\\"'`~@#$%^&*=_|<> ",
      &charsCount);

  Font font = LoadFontEx(font_path, font_size, chars, charsCount);
  UnloadCodepoints(chars);

  if (font.baseSize == 0) {
    WARN("Failed to load font from {}. Using default font for letters!", font_path);
    font = GetFontDefault();
    WARN("Default font may not support Cyrillic characters!");
  }

  auto generateLetterTexture = [this, &font, texture_size, font_size](const std::string& letter) {
    Image letter_img = GenImageColor(texture_size, texture_size, WHITE);

    const Vector2 text_size = MeasureTextEx(font, letter.c_str(), static_cast<float>(font_size), 0);
    const float x = (texture_size - text_size.x) / 2.0f;
    const float y = (texture_size - text_size.y) / 2.0f;

    ImageDrawTextEx(&letter_img, font, letter.data(), {x, y}, static_cast<float>(font_size), 0, BLACK);
    ImageFlipVertical(&letter_img);

    Texture2D texture = LoadTextureFromImage(letter_img);
    letter_textures_[letter] = texture;

    UnloadImage(letter_img);
  };

  for (const auto& letter : russian_letters) {
    generateLetterTexture(letter);
  }

  for (char c = '0'; c <= '9'; ++c) {
    const std::string num_str(1, c);
    generateLetterTexture(num_str);
  }

  const std::string question_mark = "?";
  generateLetterTexture(question_mark);

  UnloadFont(font);
}

void Renderer::UnloadWordTextures() {
  for (auto& [letter, texture] : letter_textures_) {
    UnloadTexture(texture);
  }
  letter_textures_.clear();
}

void Renderer::DrawHUD(Timestep delta_time, const GameState& game_state) {
  static char buffer[128];

  std::snprintf(buffer, sizeof(buffer), "FPS: %d", static_cast<uint32_t>(delta_time.GetFramerate()));

  DrawText(buffer, 10, 10, 20, WHITE);
  DrawText("ESC - Exit | F2 - Show/Hide grid\nWASD - Move | Ctrl - Speed boost | Mouse - Rotate | Scroll - FOV", 10,
           GetScreenHeight() - 50, 20, WHITE);
}

Image Renderer::CreateCubemapImage(const char* right_path, const char* left_path, const char* top_path,
                                   const char* bottom_path, const char* front_path, const char* back_path) {
  Image right = LoadImage(right_path);
  Image left = LoadImage(left_path);
  Image top = LoadImage(top_path);
  Image bottom = LoadImage(bottom_path);
  Image front = LoadImage(front_path);
  Image back = LoadImage(back_path);

  if (!right.data && !left.data && !top.data && !bottom.data && !front.data && !back.data) {
    ERROR("Failed to load one or more cubemap faces!");
    return Image{nullptr};
  }

  if (right.width != right.height || right.width != left.width || right.width != top.width ||
      right.width != bottom.width || right.width != front.width || right.width != back.width) {
    ImageResize(&right, 1024, 1024);
    ImageResize(&left, 1024, 1024);
    ImageResize(&top, 1024, 1024);
    ImageResize(&bottom, 1024, 1024);
    ImageResize(&front, 1024, 1024);
    ImageResize(&back, 1024, 1024);
  }

  const int face_size = right.width;

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

void Renderer::DrawCubeTexture(const Texture2D& texture, const Vector3& position, float width, float height,
                               float length, Color color) {
  Texture2D old_texture = cube_model_.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture;
  cube_model_.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = texture;

  DrawModelEx(cube_model_, position, {0, 1, 0}, 0.0f, {width, height, length}, color);

  cube_model_.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = std::move(old_texture);
}

void Renderer::DrawText3D(std::string_view text, const Vector3& position, float font_size, float font_spacing,
                          Color color) {
  const Font font = GetFontDefault();
  const float scale = font_size / static_cast<float>(font.baseSize);
  float text_offset_y = 0.0f;
  float text_offset_x = 0.0f;
  const int length = TextLength(text.data());

  rlSetTexture(font.texture.id);

  for (int i = 0; i < length;) {
    int codepoint_byte_count = 0;
    const int codepoint = GetCodepoint(&text.data()[i], &codepoint_byte_count);
    const int index = GetGlyphIndex(font, codepoint);

    if (codepoint == 0x3f) {
      codepoint_byte_count = 1;
    }

    if (codepoint == '\n') {
      text_offset_y += scale + font_spacing / static_cast<float>(font.baseSize) * scale;
      text_offset_x = 0.0f;
    } else {
      if ((codepoint != ' ') && (codepoint != '\t')) {
        Vector3 char_pos = {position.x + text_offset_x, position.y, position.z + text_offset_y};

        char_pos.x += static_cast<float>(font.glyphs[index].offsetX - font.glyphPadding) /
                      static_cast<float>(font.baseSize) * scale;
        char_pos.z += static_cast<float>(font.glyphs[index].offsetY - font.glyphPadding) /
                      static_cast<float>(font.baseSize) * scale;

        const Rectangle src_rec = {font.recs[index].x - static_cast<float>(font.glyphPadding),
                                   font.recs[index].y - static_cast<float>(font.glyphPadding),
                                   font.recs[index].width + 2.0f * font.glyphPadding,
                                   font.recs[index].height + 2.0f * font.glyphPadding};

        const float width = src_rec.width / static_cast<float>(font.baseSize) * scale;
        const float height = src_rec.height / static_cast<float>(font.baseSize) * scale;

        const float tx = src_rec.x / font.texture.width;
        const float ty = src_rec.y / font.texture.height;
        const float tw = (src_rec.x + src_rec.width) / font.texture.width;
        const float th = (src_rec.y + src_rec.height) / font.texture.height;

        rlPushMatrix();
        rlTranslatef(char_pos.x, char_pos.y, char_pos.z);

        rlBegin(RL_QUADS);
        rlColor4ub(color.r, color.g, color.b, color.a);

        rlNormal3f(0.0f, 1.0f, 0.0f);
        rlTexCoord2f(tx, ty);
        rlVertex3f(0.0f, 0.0f, 0.0f);
        rlTexCoord2f(tx, th);
        rlVertex3f(0.0f, 0.0f, height);
        rlTexCoord2f(tw, th);
        rlVertex3f(width, 0.0f, height);
        rlTexCoord2f(tw, ty);
        rlVertex3f(width, 0.0f, 0.0f);

        rlNormal3f(0.0f, -1.0f, 0.0f);
        rlTexCoord2f(tx, ty);
        rlVertex3f(0.0f, 0.0f, 0.0f);
        rlTexCoord2f(tw, ty);
        rlVertex3f(width, 0.0f, 0.0f);
        rlTexCoord2f(tw, th);
        rlVertex3f(width, 0.0f, height);
        rlTexCoord2f(tx, th);
        rlVertex3f(0.0f, 0.0f, height);
        rlEnd();
        rlPopMatrix();
      }

      if (font.glyphs[index].advanceX == 0) {
        text_offset_x += (font.recs[index].width + font_spacing) / static_cast<float>(font.baseSize) * scale;
      } else {
        text_offset_x += (font.glyphs[index].advanceX + font_spacing) / static_cast<float>(font.baseSize) * scale;
      }
    }

    i += codepoint_byte_count;
  }

  rlSetTexture(0);
}

Texture2D Renderer::GetOrCreateLetterTexture(const std::string& letter) {
  if (letter.empty()) {
    return letter_textures_["?"];
  }

  auto it = letter_textures_.find(letter);
  if (it != letter_textures_.end()) {
    return it->second;
  }

  if (letter.length() == 1 && letter[0] >= 'a' && letter[0] <= 'z') {
    std::string str(letter);
    str[0] = std::toupper(letter[0]);
    it = letter_textures_.find(letter);
    if (it != letter_textures_.end()) {
      return it->second;
    }
  }

  WARN("Character '{}' not found in textures, using '?' instead!", letter);
  return letter_textures_["?"];
}

Vector3 Renderer::MeasureText3D(const Font& font, std::string_view text, float font_size, float font_spacing,
                                float line_spacing) {
  const int len = TextLength(text.data());
  int temp_len = 0;
  int len_counter = 0;

  float temp_text_width = 0.0f;

  const float scale = font_size / static_cast<float>(font.baseSize);
  float text_height = scale;
  float text_width = 0.0f;

  int letter = 0;
  int index = 0;

  for (int i = 0; i < len; ++i) {
    ++len_counter;

    int next = 0;
    letter = GetCodepoint(&text[i], &next);
    index = GetGlyphIndex(font, letter);

    if (letter == 0x3f) {
      next = 1;
    }
    i += next - 1;

    if (letter != '\n') {
      if (font.glyphs[index].advanceX != 0) {
        text_width += (font.glyphs[index].advanceX + font_spacing) / static_cast<float>(font.baseSize) * scale;
      } else {
        text_width += (font.recs[index].width + font.glyphs[index].offsetX) / static_cast<float>(font.baseSize) * scale;
      }
    } else {
      if (temp_text_width < text_width) temp_text_width = text_width;
      len_counter = 0;
      text_width = 0.0f;
      text_height += scale + line_spacing / static_cast<float>(font.baseSize) * scale;
    }

    if (temp_len < len_counter) {
      temp_len = len_counter;
    }
  }

  if (temp_text_width < text_width) temp_text_width = text_width;

  const Vector3 vec = {
      temp_text_width + (static_cast<float>(temp_len - 1) * font_spacing / static_cast<float>(font.baseSize) * scale),
      0.25f, text_height};

  return vec;
}

void Renderer::CreateTestLetterCubes() {
  const std::array<std::string, 6> test_word = {"П", "Р", "И", "В", "Е", "Т"};

  for (size_t i = 0; i < test_word.size(); ++i) {
    const Vector3 position = {static_cast<float>(i) * block_size_ + 10.0f, block_size_ * 1.0f, block_size_ * 1.0f};

    test_blocks_.push_back({GetOrCreateLetterTexture(test_word[i]), position, Direction::kRight, test_word[i],
                            universal_cube_color_, 1.0f, 0.0f});
  }

  const std::array<std::string, 5> test_word2 = {"Г", "О", "Р", "О", "Д"};

  for (size_t i = 0; i < test_word2.size(); ++i) {
    const Vector3 position = {block_size_ * 5.0f, static_cast<float>(i) * block_size_ + 10.0f, block_size_ * 5.0f};

    test_blocks_.push_back({GetOrCreateLetterTexture(test_word2[i]), position, Direction::kUp, test_word2[i],
                            universal_cube_color_, 1.0f, 0.0f});
  }

  const std::array<std::string, 5> test_word3 = {"С", "Л", "О", "В", "О"};

  for (size_t i = 0; i < test_word3.size(); ++i) {
    const Vector3 position = {block_size_ * 10.0f, block_size_ * 1.0f, static_cast<float>(i) * block_size_ + 10.0f};

    test_blocks_.push_back({GetOrCreateLetterTexture(test_word3[i]), position, Direction::kForward, test_word3[i],
                            universal_cube_color_, 1.0f, 0.0f});
  }
}

}  // namespace app