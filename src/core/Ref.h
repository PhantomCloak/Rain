#pragma once

#include <memory>

namespace Rain
{
  template <typename T>
  using Ref = std::shared_ptr<T>;

  template <typename T>
  using Scope = std::unique_ptr<T>;

  template <typename T, typename... Args>
  constexpr Ref<T> CreateRef(Args&&... args)
  {
    return std::make_shared<T>(std::forward<Args>(args)...);
  }

  template <typename T, typename... Args>
  static Ref<T> Create(Args&&... args)
  {
    return std::make_shared<T>(std::forward<Args>(args)...);
  }

// Type-safe stack zero initialization
#define ZERO_INIT(obj) std::memset(static_cast<void*>(&(obj)), 0, sizeof(obj))

// Safe heap allocation with zero initialization
#define ZERO_ALLOC(type) ([&]() -> type* {                      \
  void* ptr = std::malloc(sizeof(type));                        \
  if (!ptr)                                                     \
    return nullptr;                                             \
  return static_cast<type*>(std::memset(ptr, 0, sizeof(type))); })()
}  // namespace Rain
