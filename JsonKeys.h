#pragma once
#include <algorithm>
#include <array>
#include <string_view>

constexpr std::array<std::string_view, 10> allowed_keys = {
    "age",   "content",       "email", "login", "password",
    "phone", "refresh_token", "role",  "title", "username"};

inline bool isValidKey(std::string_view key) {
  return std::ranges::binary_search(allowed_keys, key);
}