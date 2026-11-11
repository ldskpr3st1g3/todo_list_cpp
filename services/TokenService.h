#pragma once
#include "RefreshTokens.h"
#include "repositories/RefreshRepository.h"
#include <drogon/drogon.h>
#include <jwt-cpp/jwt.h>

class TokenService {
  std::shared_ptr<RefreshRepository> token_repo_ptr;
  auto generateRefreshTokenString() -> std::string;

public:
  explicit TokenService(std::shared_ptr<RefreshRepository> repo)
      : token_repo_ptr(std::move(repo)) {}

  auto generateAccessTokenString(std::string& token, int32_t pk) -> void;

  auto getRefreshString(int32_t pk) -> drogon_model::notes_db::RefreshTokens;

  auto insertRefreshToken(drogon_model::notes_db::RefreshTokens&& token) -> drogon::Task<void>;

  auto generateTokens(int32_t pk) -> drogon::Task<std::optional<Json::Value>>;
};