#pragma once

#include "Core.h"

#include <chrono>
#include <type_traits>

namespace app::utils {

class Timer {
public:
  Timer() noexcept { Reset(); }
  Timer(const Timer&) noexcept = default;
  Timer(Timer&&) noexcept = default;
  ~Timer() noexcept = default;

  void Reset() noexcept { time_stamp_ = std::chrono::steady_clock::now(); }

  inline Timer& operator=(const Timer&) noexcept = default;
  inline Timer& operator=(Timer&&) noexcept = default;

  template <typename Type = std::chrono::steady_clock::duration::rep, typename Units = std::chrono::nanoseconds>
    requires std::is_arithmetic_v<Type> &&
             std::is_same_v<Units, std::chrono::duration<typename Units::rep, typename Units::period>>
  inline Type GetElapsed() const {
    return static_cast<Type>(std::chrono::duration_cast<Units>(std::chrono::steady_clock::now() - time_stamp_).count());
  }

  inline double GetElapsedSec() const { return GetElapsed<double, std::chrono::duration<double>>(); }

  inline double GetElapsedMilliSec() const { return GetElapsed<double, std::chrono::duration<double, std::milli>>(); }

  inline uint64_t GetElapsedMicroSec() const { return GetElapsed<uint64_t, std::chrono::microseconds>(); }

  inline uint64_t GetElapsedNanoSec() const { return GetElapsed<uint64_t, std::chrono::nanoseconds>(); }

private:
  std::chrono::steady_clock::time_point time_stamp_;
};

}  // namespace app::utils