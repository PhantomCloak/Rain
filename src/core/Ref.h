#pragma once

#include <memory>
template <typename T>
using Ref = std::shared_ptr<T>;

template <typename T>
using Scope = std::unique_ptr<T>;

template <typename T, typename... Args>
constexpr Ref<T> CreateRef(Args&&... args) {
    return std::make_shared<T>(std::forward<Args>(args)...);
}
#define ZERO_INIT(obj) std::memset(&(obj), 0, sizeof(obj))
#define ZERO_ALLOC(type) (type*)std::memset(std::malloc(sizeof(type)), 0, sizeof(type))

