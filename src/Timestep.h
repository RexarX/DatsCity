#pragma once

#include "Core.h"

namespace app {

class Timestep {
public:
  constexpr Timestep() noexcept = default;
  constexpr Timestep(float time) noexcept : time_(time) {}
  constexpr Timestep(const Timestep&) noexcept = default;
  constexpr Timestep(Timestep&&) noexcept = default;
  constexpr ~Timestep() noexcept = default;

  constexpr Timestep& operator=(const Timestep&) noexcept = default;
  constexpr Timestep& operator=(Timestep&&) noexcept = default;

  constexpr operator float() const noexcept { return time_; }

  constexpr float GetSeconds() const noexcept { return time_; }
  constexpr float GetMilliseconds() const noexcept { return time_ * 1000.0f; }
  constexpr float GetFramerate() const noexcept { return 1.0f / time_; }

private:
  float time_ = 0.0f;
};

}  // namespace app