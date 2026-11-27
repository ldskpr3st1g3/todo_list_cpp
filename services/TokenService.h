#pragma once
#include "RefreshTokens.h"
#include "repositories/RefreshRepository.h"
#include <drogon/HttpTypes.h>
#include <drogon/drogon.h>
#include <jwt-cpp/jwt.h>

struct TokenBundle {
  drogon_model::notes_db::RefreshTokens refresh_token;
  Json::Value tokens;
};

class TokenService {
  std::shared_ptr<RefreshRepository> token_repo_ptr;
  auto generateRefreshTokenString() -> std::string;

public:
  explicit TokenService(std::shared_ptr<RefreshRepository> repo)
      : token_repo_ptr(std::move(repo)) {}

  auto generateAccessTokenString(std::string &token, int32_t pk) -> void;

  auto getRefreshString(int32_t pk) -> drogon_model::notes_db::RefreshTokens;

  auto insertRefreshToken(drogon_model::notes_db::RefreshTokens &&token)
      -> drogon::Task<void>;

  auto generateTokens(int32_t pk) -> drogon::Task<std::optional<Json::Value>>;

  auto RotateTokens(std::string &&refresh_token) -> drogon::Task<
      std::pair<std::optional<Json::Value>, drogon::HttpStatusCode>>;

  auto SignOutFromAllDevices(int32_t user_id) -> drogon::Task<bool>;

private:
  auto BuildTokens(int32_t user_id) -> TokenBundle;
};
