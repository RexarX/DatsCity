#pragma once

#include "Core.h"

#include <expected>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace app::utils {

static std::expected<std::string, const char*> ReadFileToString(std::string_view filepath) {
  std::ifstream in(filepath.data(), std::ios::in | std::ios::binary);
  if (!in) {
    return std::unexpected("Could not open file");
  }

  std::string result;
  in.seekg(0, std::ios::end);
  result.resize(in.tellg());
  in.seekg(0, std::ios::beg);
  in.read(result.data(), result.size());
  in.close();

  return result;
}

static std::expected<std::string, const char*> ReadFileToString(std::filesystem::path& filepath) {
  std::ifstream in(filepath, std::ios::in | std::ios::binary);
  if (!in) {
    return std::unexpected("Could not open file");
  }

  std::string result;
  in.seekg(0, std::ios::end);
  result.resize(in.tellg());
  in.seekg(0, std::ios::beg);
  in.read(result.data(), result.size());
  in.close();

  return result;
}

static constexpr std::string_view GetFileName(std::string_view path) {
  size_t last_slash = path.find_last_of("/\\");
  return (last_slash != std::string_view::npos) ? path.substr(++last_slash) : path;
}

/* Returns ".extension" */
static constexpr std::string_view GetFileExtension(std::string_view path) {
  const size_t last_dot = path.find_last_of('.');
  if (last_dot == std::string_view::npos) {
    return "";
  }
  return path.substr(last_dot);
}

}  // namespace app::utils