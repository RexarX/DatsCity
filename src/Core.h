#pragma once

#ifndef RELEASE_MODE
#define ENABLE_ASSERTS
#endif

#ifdef ENABLE_ASSERTS
#ifdef _MSC_VER
#define DEBUG_BREAK() __debugbreak()
#else
#define DEBUG_BREAK() __builtin_trap()
#endif

#define ASSERT(x, ...)  \
  if (!(x)) {           \
    ERROR(__VA_ARGS__); \
    DEBUG_BREAK();      \
  }
#define ASSERT_CRITICAL(x, ...)                    \
  if (!(x)) {                                      \
    CRITICAL("Assertion Failed: {}", __VA_ARGS__); \
    DEBUG_BREAK();                                 \
  }
#else
#define ASSERT(x, ...)                          \
  if (!(x)) {                                   \
    ERROR("Assertion Failed: {}", __VA_ARGS__); \
  }
#define ASSERT_CRITICAL(x, ...)                    \
  if (!(x)) {                                      \
    CRITICAL("Assertion Failed: {}", __VA_ARGS__); \
  }
#endif

#define BIT(x) (1 << x)

#define BIND_FN(fn) std::bind(&fn, this, std::placeholders::_1)
#define BIND_FN_WITH_REF(fn, reference) std::bind(&fn, this, std::ref(reference), std::placeholders::_1)