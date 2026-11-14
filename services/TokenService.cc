#include "services/TokenService.h"
#include "ErrorLogger.h"
#include "RefreshTokens.h"
#include <drogon/drogon.h>
#include <sodium.h>

auto TokenService::generateAccessTokenString(std::string& token, int32_t pk) -> void {
  token = jwt::create()
              .set_issuer("DEFECT")
              .set_type("JWT")
              .set_payload_claim("sub", jwt::claim(std::to_string(pk)))
              .set_expires_at(std::chrono::system_clock::now() +
                              std::chrono::minutes(
                                  drogon::app()
                                      .getCustomConfig()["jwt"]["access_token_expiration_minutes"]
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

auto TokenService::getRefreshString(int32_t pk) -> drogon_model::notes_db::RefreshTokens {
  std::string newRefreshTokenString = generateRefreshTokenString();
  drogon_model::notes_db::RefreshTokens refresh_token;
  refresh_token.setToken(newRefreshTokenString);
  refresh_token.setUserId(pk);
  refresh_token.setExpiresAt(trantor::Date::now().after(30 * 24 * 3600));
  return refresh_token;
}

auto TokenService::insertRefreshToken(drogon_model::notes_db::RefreshTokens&& token)
    -> drogon::Task<void> {
  try {
    co_await token_repo_ptr->insert(std::move(token));
  } catch (const drogon::orm::DrogonDbException& e) {
    ErrorLogger::printDatabaseError(__func__, e);
    throw;
  }
}

auto TokenService::generateTokens(int32_t pk) -> drogon::Task<std::optional<Json::Value>> {
  std::string access_token;

  try {
    generateAccessTokenString(access_token, pk);
  } catch (...) {
    co_return std::nullopt;
  }
  auto refresh_token = getRefreshString(pk);
  auto refresh_token_string = refresh_token.getValueOfToken();
  try {
    co_await insertRefreshToken(std::move(refresh_token));
  } catch (...) {
    co_return std::nullopt;
  }
  Json::Value json;
  json["access_token"] = access_token;
  json["refresh_token"] = refresh_token_string;
  json["token_type"] = "Bearer";
  co_return json;
}
