#include "Log.h"
#include "pch.h"

#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

class star_formatter_flag final : public spdlog::custom_flag_formatter {
public:
  void format(const spdlog::details::log_msg& msg, const std::tm&, spdlog::memory_buf_t& dest) override {
    if (msg.level == spdlog::level::err || msg.level == spdlog::level::critical) {
      std::array<char, 128> buffer;
      char* ptr = buffer.data();

      constexpr const char* left_bracket = "[";
      ptr = std::copy(left_bracket, left_bracket + 1, ptr);
      ptr = std::copy(msg.source.filename, msg.source.filename + std::strlen(msg.source.filename), ptr);
      *ptr++ = ':';

      const auto [line, error] = std::to_chars(ptr, buffer.data() + buffer.size(), msg.source.line);
      if (error != std::errc{}) {
        ERROR("Error occurred while writing to buffer!");
        return;
      }

      ptr = line;
      *ptr++ = ']';

      dest.append(buffer.data(), ptr);
    }
  }

  inline std::unique_ptr<custom_flag_formatter> clone() const override {
    return spdlog::details::make_unique<star_formatter_flag>();
  }
};

namespace app {

void Log::Init() {
  const auto now = std::chrono::system_clock::now();
  const auto in_time_t = std::chrono::system_clock::to_time_t(now);
  std::stringstream ss;
  ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d_%H-%M-%S");
  std::string time = ss.str();

  std::filesystem::create_directories("Logs/App");
  std::filesystem::create_directories("Logs/Server");

  std::string log_file_path = std::format("Logs/App/{}.log", time);
  logger_ = CreateNamedLogger("APP", log_file_path);

  log_file_path = std::format("Logs/Server/{}.log", time);
  server_logger_ = CreateNamedLogger("SERVER", log_file_path);
}

std::shared_ptr<spdlog::logger> Log::CreateNamedLogger(std::string_view name, std::string_view log_file_path) {
  std::vector<spdlog::sink_ptr> log_sinks;

  auto file_formatter = std::make_unique<spdlog::pattern_formatter>();
  file_formatter->add_flag<star_formatter_flag>('*').set_pattern("[%T] [%l] %n: %v %*");

  auto console_formatter = std::make_unique<spdlog::pattern_formatter>();
  console_formatter->add_flag<star_formatter_flag>('*').set_pattern("[%T] [%^%l%$] %n: %v %*");

  log_sinks.emplace_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_file_path.data(), true))
      ->set_formatter(std::move(file_formatter));

#ifndef RELEASE_MODE
  auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
  console_sink->set_formatter(std::move(console_formatter));
  log_sinks.emplace_back(std::move(console_sink));
#endif

  auto logger = std::make_shared<spdlog::logger>(name.data(), log_sinks.begin(), log_sinks.end());
  spdlog::register_logger(logger);
  logger->set_level(spdlog::level::trace);
  logger->flush_on(spdlog::level::trace);

  return logger;
}

}  // namespace app