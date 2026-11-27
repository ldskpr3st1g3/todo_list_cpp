#include "services/TokenService.h"
#include "ErrorLogger.h"
#include "RefreshTokens.h"
#include <drogon/HttpTypes.h>
#include <drogon/drogon.h>
#include <drogon/orm/Exception.h>
#include <sodium.h>
#include <trantor/utils/Date.h>

auto TokenService::generateAccessTokenString(std::string &token, int32_t pk)
    -> void {
  token =
      jwt::create()
          .set_issuer("DEFECT")
          .set_type("JWT")
          .set_payload_claim("sub", jwt::claim(std::to_string(pk)))
          .set_expires_at(
              std::chrono::system_clock::now() +
              std::chrono::minutes(
                  drogon::app()
                      .getCustomConfig()["jwt"]
                                        ["access_token_expiration_minutes"]
                      .asInt64()))
          .sign(jwt::algorithm::hs256{
              drogon::app().getCustomConfig()["jwt"]["secret_key"].asString()});
}

auto TokenService::generateRefreshTokenString() -> std::string {
  std::array<uint8_t, 64> random_bytes{};
  randombytes_buf(random_bytes.data(), random_bytes.size());
  std::string token;
  token.reserve(128);
  for (uint8_t byte : random_bytes) {
    char buf[3];
    snprintf(buf, sizeof(buf), "%02x", byte);
    token.append(buf);
  }
  return token;
}

auto TokenService::getRefreshString(int32_t pk)
    -> drogon_model::notes_db::RefreshTokens {
  std::string newRefreshTokenString = generateRefreshTokenString();
  drogon_model::notes_db::RefreshTokens refresh_token;
  refresh_token.setToken(newRefreshTokenString);
  refresh_token.setUserId(pk);
  refresh_token.setExpiresAt(trantor::Date::now().after(30 * 24 * 3600));
  return refresh_token;
}

auto TokenService::insertRefreshToken(
    drogon_model::notes_db::RefreshTokens &&token) -> drogon::Task<void> {
  try {
    co_await token_repo_ptr->insert(std::move(token));
  } catch (const drogon::orm::DrogonDbException &e) {
    ErrorLogger::printDatabaseError(__func__, e);
    throw;
  }
}

auto TokenService::BuildTokens(int32_t user_id) -> TokenBundle {
  std::string access_token;
  generateAccessTokenString(access_token, user_id);
  auto refresh_token = getRefreshString(user_id);
  Json::Value output;
  output["access_token"] = std::move(access_token);
  output["refresh_token"] = refresh_token.getValueOfToken();
  output["token_type"] = "Bearer";
  return {std::move(refresh_token), std::move(output)};
}

auto TokenService::generateTokens(int32_t pk)
    -> drogon::Task<std::optional<Json::Value>> {
  auto [refresh_token, tokens] = BuildTokens(pk);

  try {
    co_await insertRefreshToken(std::move(refresh_token));
  } catch (const drogon::orm::DrogonDbException &e) {
    ErrorLogger::printDatabaseError(__func__, e);
    co_return std::nullopt;
  }
  co_return tokens;
}

auto TokenService::RotateTokens(std::string &&refresh_token_string)
    -> drogon::Task<
        std::pair<std::optional<Json::Value>, drogon::HttpStatusCode>> {
  drogon_model::notes_db::RefreshTokens old_token;
  try {
    old_token = co_await token_repo_ptr->GetRefreshToken(
        std::move(refresh_token_string));
  } catch (const drogon::orm::UnexpectedRows &e) {
    co_return std::make_pair(std::nullopt,
                             drogon::HttpStatusCode::k401Unauthorized);
  } catch (const drogon::orm::DrogonDbException &e) {
    co_return std::make_pair(std::nullopt,
                             drogon::HttpStatusCode::k500InternalServerError);
  }
  if (old_token.getValueOfIsRevoked() ||
      old_token.getValueOfExpiresAt() <= trantor::Date::now())
    co_return std::make_pair(std::nullopt,
                             drogon::HttpStatusCode::k403Forbidden);
  int32_t user_id = old_token.getValueOfUserId();
  auto [new_refresh_token, tokens] = BuildTokens(user_id);
  try {
    co_await token_repo_ptr->RotateTokens(std::move(old_token),
                                          std::move(new_refresh_token));
  } catch (const drogon::orm::DrogonDbException &e) {
    ErrorLogger::printDatabaseError(__func__, e);
    co_return std::make_pair(std::nullopt,
                             drogon::HttpStatusCode::k500InternalServerError);
  }
  co_return std::make_pair(std::move(tokens), drogon::HttpStatusCode::k200OK);
}

auto TokenService::SignOutFromAllDevices(int32_t user_id)
    -> drogon::Task<bool> {
  try {
    co_await token_repo_ptr->SignOutFromAllDevices(user_id);
    co_return true;
  } catch (const drogon::orm::DrogonDbException &e) {
    ErrorLogger::printDatabaseError(__func__, e);
    co_return false;
  }
}
