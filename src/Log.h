#pragma once

#include <source_location>

#ifdef PLATFORM_WINDOWS
#define NOGDI
#define NOUSER
#endif

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

#include "Core.h"

namespace app {

class Log {
public:
  static void Init();

  template <typename... Args>
  static void LogMessage(const std::shared_ptr<spdlog::logger>& logger, spdlog::level::level_enum level,
                         const std::source_location& loc, const spdlog::fmt_lib::format_string<Args...>& fmt,
                         Args&&... args) {
    std::string_view filename(loc.file_name());
    const size_t last_slash = filename.find_last_of("/\\");
    if (last_slash != std::string_view::npos) {
      filename = filename.substr(last_slash + 1);
    }

    logger->log(spdlog::source_loc{filename.data(), static_cast<int>(loc.line()), nullptr}, level,
                spdlog::fmt_lib::format(fmt, std::forward<Args>(args)...));
  }

  static void LogMessage(const std::shared_ptr<spdlog::logger>& logger, spdlog::level::level_enum level,
                         const std::source_location& loc, std::string_view string) {
    std::string_view filename(loc.file_name());
    const size_t last_slash = filename.find_last_of("/\\");
    if (last_slash != std::string_view::npos) {
      filename = filename.substr(last_slash + 1);
    }

    logger->log(spdlog::source_loc{filename.data(), static_cast<int>(loc.line()), nullptr}, level, string);
  }

  static inline std::shared_ptr<spdlog::logger>& GetLogger() noexcept { return logger_; }
  static inline std::shared_ptr<spdlog::logger>& GetServerLogger() noexcept { return server_logger_; }

private:
  static std::shared_ptr<spdlog::logger> CreateNamedLogger(std::string_view name, std::string_view log_file_path);

private:
  static inline std::shared_ptr<spdlog::logger> logger_ = nullptr;
  static inline std::shared_ptr<spdlog::logger> server_logger_ = nullptr;
};

}  // namespace app

#define TRACE(...) ::app::Log::GetLogger()->trace(__VA_ARGS__)
#define INFO(...) ::app::Log::GetLogger()->info(__VA_ARGS__)
#define WARN(...) ::app::Log::GetLogger()->warn(__VA_ARGS__)
#define ERROR(...) \
  ::app::Log::LogMessage(::app::Log::GetLogger(), spdlog::level::err, std::source_location::current(), __VA_ARGS__)
#define CRITICAL(...) \
  ::app::Log::LogMessage(::app::Log::GetLogger(), spdlog::level::critical, std::source_location::current(), __VA_ARGS__)

#define SERVER_TRACE(...) ::app::Log::GetServerLogger()->trace(__VA_ARGS__)
#define SERVER_INFO(...) ::app::Log::GetServerLogger()->info(__VA_ARGS__)
#define SERVER_WARN(...) ::app::Log::GetServerLogger()->warn(__VA_ARGS__)
#define SERVER_ERROR(...)                                                                                    \
  ::app::Log::LogMessage(::app::Log::GetServerLogger(), spdlog::level::err, std::source_location::current(), \
                         __VA_ARGS__)
#define SERVER_CRITICAL(...)                                                                                      \
  ::app::Log::LogMessage(::app::Log::GetServerLogger(), spdlog::level::critical, std::source_location::current(), \
                         __VA_ARGS__)