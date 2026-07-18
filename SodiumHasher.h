#pragma once
#include <array>
#include <drogon/drogon.h>
#include <optional>
#include <sodium.h>
#include <sodium/crypto_pwhash.h>
#include <string>
#include <string_view>

namespace Hasher {
inline auto hash(std::string_view password) -> std::optional<std::string> {
  std::array<char, crypto_pwhash_STRBYTES> buffer{};
  if (crypto_pwhash_str(buffer.data(), password.data(), password.size(),
                        crypto_pwhash_opslimit_interactive(), crypto_pwhash_memlimit_interactive()))
    return std::nullopt;
  std::string password_hash(buffer.data());
  sodium_memzero(buffer.data(), buffer.size());
  return password_hash;
}

inline auto verify(std::string_view password, std::string_view password_hash) -> bool {
  return !crypto_pwhash_str_verify(password_hash.data(), password.data(), password.size());
}
} // namespace Hasher