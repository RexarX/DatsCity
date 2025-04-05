#include "Renderer.h"

#include <raymath.h>
#include <rlgl.h>

#if defined(PLATFORM_DESKTOP)
const int GLSL_VERSION = 330;
#else
const int GLSL_VERSION = 100;
#endif

constexpr std::array<std::string_view, 33> russian_letters = {"А", "Б", "В", "Г", "Д", "Е", "Ё", "Ж", "З", "И", "Й",
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
    DrawCubeTexture(block.texture, block.position, block_size_, block_size_, block_size_, block.color);
    DrawSphere(block.position, 0.5f, {255, 255, 0, 200});
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

  // Add active tower words to visualization
  if (game_state.towers_data.tower.has_value()) {
    const auto& tower = game_state.towers_data.tower.value();
    for (const auto& word : tower.words) {
      const Vector3 pos = {static_cast<float>(word.pos.x) * block_size_, static_cast<float>(word.pos.y) * block_size_,
                           static_cast<float>(word.pos.z) * block_size_};

      // Process each letter in the word (safely handling UTF-8)
      size_t i = 0;
      size_t byte_pos = 0;

      while (byte_pos < word.text.length()) {
        // Get the next UTF-8 character
        int char_size = 0;
        // Check the first byte to determine multi-byte sequence length
        const uint8_t c = static_cast<uint8_t>(word.text[byte_pos]);

        if ((c & 0x80) == 0) {
          // ASCII character (1 byte)
          char_size = 1;
        } else if ((c & 0xE0) == 0xC0) {
          // 2-byte sequence
          char_size = 2;
        } else if ((c & 0xF0) == 0xE0) {
          // 3-byte sequence
          char_size = 3;
        } else if ((c & 0xF8) == 0xF0) {
          // 4-byte sequence
          char_size = 4;
        } else {
          // Invalid UTF-8, skip this byte
          ++byte_pos;
          continue;
        }

        if (byte_pos + char_size > word.text.length()) {
          break;
        }

        // Extract the UTF-8 character
        std::string letter = word.text.substr(byte_pos, char_size);

        // Calculate position for this letter
        const Coords offset = DirectionToCoords(word.dir);
        const Vector3 letter_pos = {pos.x + offset.x * block_size_ * static_cast<float>(i),
                                    pos.y + offset.y * block_size_ * static_cast<float>(i),
                                    pos.z + offset.z * block_size_ * static_cast<float>(i)};

        // Add the letter cube to our collection
        word_blocks_.push_back(
            {GetOrCreateLetterTexture(letter), letter_pos, word.dir, letter, active_word_color_, 1.0f, 0.0f});

        // Move to the next character
        byte_pos += char_size;
        ++i;
      }
    }
  }

  // Add completed towers words
  for (const auto& done_tower : game_state.towers_data.done_towers) {
    // This is a placeholder for when that data is available
  }

  // Draw all word blocks
  for (const auto& block : word_blocks_) {
    DrawCubeTexture(block.texture, block.position, block_size_, block_size_, block_size_, block.color);
    // For debugging, also draw the letter position as a small sphere
    DrawSphere(block.position, 0.5f, RED);
  }
}

void Renderer::DrawMapGrid(const Coords& map_size) {
  // Calculate dimensions of the map in world units
  const float width = block_size_ * map_size.x;
  const float height = block_size_ * map_size.y;
  const float depth = block_size_ * map_size.z;
  const float size = std::min({width, height, depth}) / 100;

  // Set colors for the different axes and grid elements
  const Color x_axis_color = {255, 50, 50, 255};  // Red for X
  const Color y_axis_color = {50, 255, 50, 255};  // Green for Y
  const Color z_axis_color = {50, 50, 255, 255};  // Blue for Z
  const Color grid_line_color = {180, 180, 180, 120};

  // Draw main coordinate axes
  DrawLine3D({0, 0, 0}, {size + 10.0f, 0, 0}, x_axis_color);    // X axis
  DrawLine3D({0, 0, 0}, {0, size + 10.0f, 0}, y_axis_color);   // Y axis
  DrawLine3D({0, 0, 0}, {0, 0, size + 10.0f}, z_axis_color);    // Z axis

  // Draw axis labels
  DrawText3D("X", {size + 12.0f, 0, 0}, 20.0f, 1.0f, x_axis_color);
  DrawText3D("Y", {0, size + 12.0f, 0}, 20.0f, 1.0f, y_axis_color);
  DrawText3D("Z", {0, 0, size + 12.0f}, 20.0f, 1.0f, z_axis_color);

  rlPushMatrix();
  rlRotatef(90.0f, 1.0f, 0.0f, 0.0f);  // Align text to be horizontal

  constexpr std::string_view title = "DatsCity";
  const Vector3 text_size = MeasureText3D(GetFontDefault(), title.data(), 30.0f, 2.0f, 0.0f);
  const Vector3 text_pos = {
      width / 2.0f - text_size.x / 2.0f,  // Center horizontally
      5.0f,                               // Slightly above the grid
      depth / 2.0f - text_size.z / 2.0f   // Center vertically
  };

  DrawText3D(title, text_pos, 30.0f, 2.0f, RED);
  rlPopMatrix();

  // Draw grid wireframe
  DrawCubeWiresV({width / 2, height / 2, depth / 2}, {width, height, depth}, grid_line_color);

  // Draw the dimensions label
  /*char dimensions[50];
  sprintf(dimensions, "Map size: %d x %d x %d", map_size.x, map_size.y, map_size.z);
  DrawBillboardText(dimensions, {width / 2, height + 30.0f, depth / 2}, 24.0f, WHITE);*/
}

void Renderer::GenerateLetterTextures() {
  // Generate textures for Russian alphabet and numbers
  constexpr int texture_size = 128;
  constexpr int font_size = 96;
  constexpr const char* font_path = "Assets/Fonts/Roboto-Bold.ttf";

  // Make sure font directory exists
  if (!DirectoryExists("Assets/Fonts")) {
    INFO("Creating Assets/Fonts directory.");
    std::filesystem::create_directories("Assets/Fonts");
    INFO("Created Assets/Fonts directory - please add a font file there.");
    return;
  }

  // Load a font that supports Cyrillic characters
  Font font = LoadFontEx(font_path, font_size, nullptr, 0);
  if (font.baseSize == 0) {
    WARN("Failed to load font from {}. Using default font for letters!", font_path);
    font = GetFontDefault();
    WARN("Default font may not support Cyrillic characters!");
  }

  // Function to generate texture for a single character
  auto generateLetterTexture = [&](std::string_view letter) {
    Image letter_img = GenImageColor(texture_size, texture_size, WHITE);

    // Create the centered letter text
    const Vector2 text_size = MeasureTextEx(font, letter.data(), static_cast<float>(font_size), 0);
    const float x = (texture_size - text_size.x) / 2.0f;
    const float y = (texture_size - text_size.y) / 2.0f;

    // Draw the letter on the image
    ImageDrawTextEx(&letter_img, font, letter.data(), {x, y}, static_cast<float>(font_size), 0, BLACK);

    // Flip the image vertically to correct orientation when rendered on cube
    ImageFlipVertical(&letter_img);

    // Create texture from the image
    const Texture2D texture = LoadTextureFromImage(letter_img);
    letter_textures_[letter] = texture;

    UnloadImage(letter_img);
  };

  // Generate textures for all Russian letters
  for (const auto& letter : russian_letters) {
    generateLetterTexture(letter);
    INFO("Generated texture for letter: {}.", letter);
  }

  // Generate textures for numbers
  for (char c = '0'; c <= '9'; ++c) {
    generateLetterTexture(std::string(1, c));
  }

  // Add a default texture for unknown characters
  Image default_img = GenImageColor(texture_size, texture_size, WHITE);

  // Draw a question mark in the center
  const char* question_mark = "?";
  const Vector2 qm_size = MeasureTextEx(font, question_mark, static_cast<float>(font_size), 0);
  const float qm_x = (texture_size - qm_size.x) / 2.0f;
  const float qm_y = (texture_size - qm_size.y) / 2.0f;

  ImageDrawTextEx(&default_img, font, question_mark, {qm_x, qm_y}, static_cast<float>(font_size), 0, BLACK);

  // Create texture from the image
  const Texture2D default_texture = LoadTextureFromImage(default_img);
  letter_textures_["?"] = default_texture;

  UnloadImage(default_img);
  UnloadFont(font);

  INFO("Generated textures for Russian alphabet (А-Я), numbers 0-9, and default '?'.");
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

void Renderer::DrawCubeTexture(const Texture2D& texture, const Vector3& position, float width, float height,
                               float length, Color color) {
  // Save current model texture
  Texture2D old_texture = cube_model_.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture;

  cube_model_.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = texture;

  // Set model transformation matrix
  const Matrix scale = MatrixScale(width, height, length);
  const Matrix translation = MatrixTranslate(position.x, position.y, position.z);
  const Matrix transform = MatrixMultiply(scale, translation);

  // Draw the cube with texture
  DrawModelEx(cube_model_, position, {0, 1, 0}, 0.0f, {width, height, length}, color);

  // Restore previous texture
  cube_model_.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = std::move(old_texture);
}

void Renderer::DrawText3D(std::string_view text, const Vector3& position, float font_size, float font_spacing,
                          Color color) {
  // Load font (consider caching this)
  const Font font = GetFontDefault();

  // Calculate scale based on font size
  const float scale = font_size / static_cast<float>(font.baseSize);

  // Calculate offsets
  float text_offset_y = 0.0f;
  float text_offset_x = 0.0f;

  // Get text length (UTF-8 aware)
  const int length = TextLength(text.data());

  // Set font texture
  rlSetTexture(font.texture.id);

  // Iterate through characters
  for (int i = 0; i < length;) {
    // Get next codepoint from byte string and glyph index in font
    int codepoint_byte_count = 0;
    const int codepoint = GetCodepoint(&text.data()[i], &codepoint_byte_count);
    const int index = GetGlyphIndex(font, codepoint);

    // Handle invalid characters
    if (codepoint == 0x3f) {
      codepoint_byte_count = 1;
    }

    if (codepoint == '\n') {
      // Handle newline
      text_offset_y += scale + font_spacing / static_cast<float>(font.baseSize) * scale;
      text_offset_x = 0.0f;
    } else {
      if ((codepoint != ' ') && (codepoint != '\t')) {
        // Calculate character position with offsets
        Vector3 char_pos = {position.x + text_offset_x, position.y, position.z + text_offset_y};

        // Apply glyph offset and padding
        char_pos.x += static_cast<float>(font.glyphs[index].offsetX - font.glyphPadding) /
                      static_cast<float>(font.baseSize) * scale;
        char_pos.z += static_cast<float>(font.glyphs[index].offsetY - font.glyphPadding) /
                      static_cast<float>(font.baseSize) * scale;

        // Get source rectangle from font atlas
        const Rectangle src_rec = {font.recs[index].x - static_cast<float>(font.glyphPadding),
                             font.recs[index].y - static_cast<float>(font.glyphPadding),
                             font.recs[index].width + 2.0f * font.glyphPadding,
                             font.recs[index].height + 2.0f * font.glyphPadding};

        // Calculate width and height
        const float width = src_rec.width / static_cast<float>(font.baseSize) * scale;
        const float height = src_rec.height / static_cast<float>(font.baseSize) * scale;

        // Calculate texture coordinates
        const float tx = src_rec.x / font.texture.width;
        const float ty = src_rec.y / font.texture.height;
        const float tw = (src_rec.x + src_rec.width) / font.texture.width;
        const float th = (src_rec.y + src_rec.height) / font.texture.height;

        // Draw the character as a quad
        rlPushMatrix();
        rlTranslatef(char_pos.x, char_pos.y, char_pos.z);

        rlBegin(RL_QUADS);
        rlColor4ub(color.r, color.g, color.b, color.a);

        // Front face
        rlNormal3f(0.0f, 1.0f, 0.0f);  // Normal pointing up
        rlTexCoord2f(tx, ty);
        rlVertex3f(0.0f, 0.0f, 0.0f);  // Top-left
        rlTexCoord2f(tx, th);
        rlVertex3f(0.0f, 0.0f, height);  // Bottom-left
        rlTexCoord2f(tw, th);
        rlVertex3f(width, 0.0f, height);  // Bottom-right
        rlTexCoord2f(tw, ty);
        rlVertex3f(width, 0.0f, 0.0f);  // Top-right

        // Back face (optional, for visibility from behind)
        rlNormal3f(0.0f, -1.0f, 0.0f);  // Normal pointing down
        rlTexCoord2f(tx, ty);
        rlVertex3f(0.0f, 0.0f, 0.0f);  // Top-right
        rlTexCoord2f(tw, ty);
        rlVertex3f(width, 0.0f, 0.0f);  // Top-left
        rlTexCoord2f(tw, th);
        rlVertex3f(width, 0.0f, height);  // Bottom-left
        rlTexCoord2f(tx, th);
        rlVertex3f(0.0f, 0.0f, height);  // Bottom-right
        rlEnd();
        rlPopMatrix();
      }

      // Advance X position for next character
      if (font.glyphs[index].advanceX == 0) {
        text_offset_x += (font.recs[index].width + font_spacing) / static_cast<float>(font.baseSize) * scale;
      } else {
        text_offset_x += (font.glyphs[index].advanceX + font_spacing) / static_cast<float>(font.baseSize) * scale;
      }
    }

    i += codepoint_byte_count;  // Move text bytes counter to next codepoint
  }

  // Reset texture
  rlSetTexture(0);
}

void Renderer::DrawBillboardText(std::string_view text, const Vector3& position, float font_size, Color color) {
  const Font font = GetFontDefault();

  const int text_width = MeasureText(text.data(), static_cast<int>(font_size));
  const int text_height = static_cast<int>(font_size * 1.5f);

  // Create image with padding (prevent text clipping)
  const int padding = 10;
  Image text_img = GenImageColor(text_width + padding * 2, text_height + padding * 2, BLANK);

  ImageDrawText(&text_img, text.data(), padding, padding, static_cast<int>(font_size), color);

  const Texture2D text_texture = LoadTextureFromImage(text_img);

  // Calculate size based on distance for consistent appearance
  const float dist = Vector3Distance(camera_.position, position);
  const float size = (font_size / 30.0f) * (dist / 10.0f);

  DrawBillboard(camera_, text_texture, position, size, color);

  UnloadTexture(text_texture);
  UnloadImage(text_img);
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

  const int face_size = right.width;

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

Texture2D Renderer::GetOrCreateLetterTexture(std::string_view letter) {
  if (letter.empty()) {
    return letter_textures_["?"];
  }

  std::string char_str(letter);

  // Return existing texture if available
  auto it = letter_textures_.find(char_str);
  if (it != letter_textures_.end()) {
    return it->second;
  }

  // If not found, check if it's a Latin letter that needs uppercasing
  if (letter.length() == 1 && letter[0] >= 'a' && letter[0] <= 'z') {
    // Convert Latin letter to uppercase
    char_str.front() = std::toupper(char_str.front());
    it = letter_textures_.find(char_str);
    if (it != letter_textures_.end()) {
      return it->second;
    }
  }

  // Return the default '?' texture for unknown characters
  return letter_textures_["?"];
}

Vector3 Renderer::MeasureText3D(const Font& font, std::string_view text, float font_size, float font_spacing, float line_spacing) {
  const int len = TextLength(text.data());
  int temp_len = 0;  // Used to count longer text line num chars
  int len_counter = 0;

  float temp_text_width = 0.0f;  // Used to count longer text line width

  const float scale = font_size / static_cast<float>(font.baseSize);
  float text_height = scale;
  float text_width = 0.0f;

  int letter = 0;  // Current character
  int index = 0;   // Index position in sprite font

  for (int i = 0; i < len; ++i) {
    ++len_counter;

    int next = 0;
    letter = GetCodepoint(&text[i], &next);
    index = GetGlyphIndex(font, letter);

    // Handle invalid characters
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
  INFO("Creating test letter cubes with Russian alphabet.");

  // Create a test word with Russian letters: "ПРИВЕТ"
  const std::vector<std::string> test_word = {"П", "Р", "И", "В", "Е", "Т"};

  // Place them along the X axis
  for (size_t i = 0; i < test_word.size(); ++i) {
    const Vector3 position = {
        static_cast<float>(i) * block_size_ * 1.2f + 10.0f,  // X position with small gaps
        block_size_ * 1.0f,                                  // Y position (a bit up from the ground)
        block_size_ * 1.0f                                   // Z position (a bit in from the edge)
    };

    test_blocks_.push_back(
        {GetOrCreateLetterTexture(test_word[i]), position, Direction::kRight, test_word[i], RED, 1.0f, 0.0f});
  }

  // Create a vertical stack with the word "ГОРОД"
  const std::vector<std::string> test_word2 = {"Г", "О", "Р", "О", "Д"};

  // Place them along the Y axis
  for (size_t i = 0; i < test_word2.size(); ++i) {
    const Vector3 position = {
        block_size_ * 5.0f,                                  // X position
        static_cast<float>(i) * block_size_ * 1.2f + 10.0f,  // Y position with small gaps
        block_size_ * 5.0f                                   // Z position
    };

    test_blocks_.push_back(
        {GetOrCreateLetterTexture(test_word2[i]), position, Direction::kUp, test_word2[i], GREEN, 1.0f, 0.0f});
  }

  // Create a line along Z axis with the word "СЛОВО"
  const std::vector<std::string> test_word3 = {"С", "Л", "О", "В", "О"};

  // Place them along the Z axis
  for (size_t i = 0; i < test_word3.size(); ++i) {
    const Vector3 position = {
        block_size_ * 10.0f,                                // X position
        block_size_ * 1.0f,                                 // Y position
        static_cast<float>(i) * block_size_ * 1.2f + 10.0f  // Z position with small gaps
    };

    test_blocks_.push_back(
        {GetOrCreateLetterTexture(test_word3[i]), position, Direction::kForward, test_word3[i], BLUE, 1.0f, 0.0f});
  }

  INFO("Created {} test letter cubes", test_blocks_.size());
}

}  // namespace app