#include "UserController.h"
#include "CpuThreadPool.h"
#include "JsonBuilder.h"
#include "JsonValidate.h"
#include "PoolAwaiter.h"
#include "RefreshTokens.h"
#include "ResponseBuilder.h"
#include "SodiumHasher.h"
#include "Users.h"
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <drogon/HttpTypes.h>
#include <drogon/orm/CoroMapper.h>
#include <drogon/orm/Criteria.h>
#include <drogon/orm/Exception.h>
#include <exception>
#include <json/value.h>
#include <jwt-cpp/jwt.h>
#include <string>
#include <trantor/net/EventLoop.h>
#include <trantor/utils/Date.h>

CpuThreadPool& getGlobalPool() {
  static CpuThreadPool pool(2);
  return pool;
}

std::string generateRefreshTokenString() {
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

auto generateAccessTokenString(std::string& token, int32_t id) -> void {
  token = jwt::create()
              .set_issuer("DEFECT")
              .set_type("JWT")
              .set_payload_claim("sub", jwt::claim(std::to_string(id)))
              .set_expires_at(std::chrono::system_clock::now() +
                              std::chrono::minutes(
                                  drogon::app()
                                      .getCustomConfig()["jwt"]["access_token_expiration_minutes"]
                                      .asInt64()))
              .sign(jwt::algorithm::hs256{
                  drogon::app().getCustomConfig()["jwt"]["secret_key"].asString()});
}

auto UserController::Registration(const drogon::HttpRequestPtr request)
    -> drogon::Task<drogon::HttpResponsePtr> {
  auto* currentLoop = trantor::EventLoop::getEventLoopOfCurrentThread();
  auto json_ptr = request->getJsonObject();
  if (!json_ptr)
    co_return ResponseBuilder::createError("No json provided");
  auto errors = JsonValidate::validateJson(*json_ptr, JsonValidate::required<std::string>("login"),
                                           JsonValidate::required<std::string>("email"),
                                           JsonValidate::required<std::string>("password"));
  if (!errors.empty())
    co_return ResponseBuilder::createError("Invalid json", drogon::HttpStatusCode::k400BadRequest,
                                           JsonBuilder::createErrorJsonByVector(std::move(errors)));
  std::string password = (*json_ptr)["password"].asString();
  std::optional<std::string> password_hash;
  try {
    password_hash = co_await run_in_pool(getGlobalPool(), currentLoop,
                                         [p = std::move(password)]() { return Hasher::hash(p); });
  } catch (std::exception& error) {
    LOG_ERROR << "THREADPOOL ERROR IN HASHER" << error.what() << '\n';
    co_return ResponseBuilder::createError("Something went wrong",
                                           drogon::HttpStatusCode::k500InternalServerError);
  }
  if (!password_hash.has_value())
    co_return ResponseBuilder::createError("Fatal error",
                                           drogon::HttpStatusCode::k500InternalServerError);

  drogon_model::notes_db::Users newUser;
  newUser.updateByMasqueradedJson((*json_ptr),
                                  {"", "username", "email", "", "", "", "login", "", ""});
  newUser.setPasswordHash(password_hash.value());
  auto db = drogon::app().getDbClient();
  if (!db) {
    LOG_ERROR << "DATABASE CONNECTION ERROR";
    co_return ResponseBuilder::createError("Database connection error",
                                           drogon::HttpStatusCode::k500InternalServerError);
  }
  drogon::orm::CoroMapper<drogon_model::notes_db::Users> mapper(db);
  try {
    auto new_user = co_await mapper.insert(std::move(newUser));
    co_return ResponseBuilder::createSuccess("User created", drogon::k201Created,
                                             std::move(new_user).toJson());
  } catch (drogon::orm::DrogonDbException& error) {
    LOG_ERROR << "DATABASE ERROR: " << error.base().what() << '\n';
    std::string e = error.base().what();
    if (e.find("duplicate key") != std::string::npos)
      co_return ResponseBuilder::createError("User with this email already exists",
                                             drogon::HttpStatusCode::k409Conflict);
    co_return ResponseBuilder::createError("Database error",
                                           drogon::HttpStatusCode::k500InternalServerError);
  }
}

auto UserController::Authorization(const drogon::HttpRequestPtr request)
    -> drogon::Task<drogon::HttpResponsePtr> {
  auto* currentLoop = trantor::EventLoop::getEventLoopOfCurrentThread();
  auto json_ptr = request->getJsonObject();
  if (!json_ptr)
    co_return ResponseBuilder::createError("No json provided");
  auto errors = JsonValidate::validateJson(*json_ptr, JsonValidate::required<std::string>("email"),
                                           JsonValidate::required<std::string>("password"));
  if (!errors.empty())
    co_return ResponseBuilder::createError("Invalid json", drogon::HttpStatusCode::k400BadRequest,
                                           JsonBuilder::createErrorJsonByVector(std::move(errors)));
  std::string password = (*json_ptr)["password"].asString();
  auto db = drogon::app().getDbClient();
  if (!db) {
    LOG_ERROR << "DATABASE CONNECTION ERROR";
    co_return ResponseBuilder::createError("Database connection error",
                                           drogon::HttpStatusCode::k500InternalServerError);
  }
  drogon::orm::CoroMapper<drogon_model::notes_db::Users> mapper(db);
  drogon_model::notes_db::Users user;
  try {
    user = co_await mapper.findOne({drogon_model::notes_db::Users::Cols::_email,
                                    drogon::orm::CompareOperator::EQ,
                                    (*json_ptr)["email"].asString()});
  } catch (drogon::orm::UnexpectedRows&) {
    co_return ResponseBuilder::createError("User not found", drogon::HttpStatusCode::k404NotFound);
  } catch (drogon::orm::DrogonDbException& error) {
    LOG_ERROR << "DATABASE ERROR" << error.base().what() << '\n';
    co_return ResponseBuilder::createError("Database error",
                                           drogon::HttpStatusCode::k500InternalServerError);
  }
  std::string password_hash = *user.getPasswordHash();
  bool flag;
  try {
    flag = co_await run_in_pool(
        getGlobalPool(), currentLoop,
        [password = std::move(password), password_hash = std::move(password_hash)]() {
          return Hasher::verify(password, password_hash);
        });
  } catch (std::exception& error) {
    LOG_ERROR << "THREADPOOL ERROR" << error.what() << '\n';
    co_return ResponseBuilder::createError("Threadpool error",
                                           drogon::HttpStatusCode::k500InternalServerError);
  }
  if (!flag)
    co_return ResponseBuilder::createError("Unauthorized",
                                           drogon::HttpStatusCode::k401Unauthorized);

  std::string access_token;
  try {
    generateAccessTokenString(access_token, user.getValueOfId());
  } catch (std::exception& error) {
    LOG_ERROR << "JWT ERROR" << error.what() << '\n';
    co_return ResponseBuilder::createError("Jwt error",
                                           drogon::HttpStatusCode::k500InternalServerError);
  }
  std::string newRefreshTokenString = generateRefreshTokenString();
  drogon_model::notes_db::RefreshTokens refresh_token;
  refresh_token.setToken(newRefreshTokenString);
  refresh_token.setUserId(*user.getId());
  refresh_token.setExpiresAt(trantor::Date::now().after(30 * 24 * 3600));
  drogon::orm::CoroMapper<drogon_model::notes_db::RefreshTokens> refreshMapper(db);
  try {
    co_await refreshMapper.insert(std::move(refresh_token));
  } catch (drogon::orm::DrogonDbException& error) {
    LOG_ERROR << "DATABASE ERROR: " << error.base().what() << '\n';
    co_return ResponseBuilder::createError("Database error",
                                           drogon::HttpStatusCode::k500InternalServerError);
  }

  Json::Value json;
  json["access_token"] = std::move(access_token);
  json["refresh_token"] = std::move(newRefreshTokenString);
  json["token_type"] = "Bearer";
  co_return ResponseBuilder::createSuccess("Login successful", drogon::HttpStatusCode::k200OK,
                                           std::move(json));
}

auto UserController::Refresh(const drogon::HttpRequestPtr request)
    -> drogon::Task<drogon::HttpResponsePtr> {
  auto* currentLoop = trantor::EventLoop::getEventLoopOfCurrentThread();
  auto json_ptr = request->getJsonObject();
  if (!json_ptr)
    co_return ResponseBuilder::createError("No json provided");
  auto errors =
      JsonValidate::validateJson(*json_ptr, JsonValidate::required<std::string>("refresh_token"));
  if (!errors.empty())
    co_return ResponseBuilder::createError("Invalid json", drogon::HttpStatusCode::k400BadRequest,
                                           JsonBuilder::createErrorJsonByVector(std::move(errors)));
  auto db = drogon::app().getDbClient();
  if (!db) {
    LOG_ERROR << "DATABASE CONNECTION ERROR";
    co_return ResponseBuilder::createError("Database connection error",
                                           drogon::HttpStatusCode::k500InternalServerError);
  }
  drogon::orm::CoroMapper<drogon_model::notes_db::RefreshTokens> token_mapper(db);
  std::optional<drogon_model::notes_db::RefreshTokens> dbToken;
  try {
    dbToken = co_await token_mapper.findOne({drogon_model::notes_db::RefreshTokens::Cols::_token,
                                             drogon::orm::CompareOperator::EQ,
                                             (*json_ptr)["refresh_token"].asString()});

  } catch (drogon::orm::DrogonDbException& error) {
    LOG_ERROR << "DATABASE ERROR: " << error.base().what() << '\n';
    co_return ResponseBuilder::createError("Database error",
                                           drogon::HttpStatusCode::k500InternalServerError);
  }
  if (!dbToken.has_value())
    co_return ResponseBuilder::createError("Refresh token not found",
                                           drogon::HttpStatusCode::k404NotFound);
  if (dbToken->getValueOfIsRevoked() || dbToken->getValueOfExpiresAt() < trantor::Date::now())
    co_return ResponseBuilder::createError("Refresh token expired or revoked",
                                           drogon::HttpStatusCode::k401Unauthorized);
  std::string new_access_token;
  auto user_id = *dbToken->getUserId();
  generateAccessTokenString(new_access_token, *dbToken->getUserId());
  auto trans = co_await db->newTransactionCoro();
  try {
    dbToken->setIsRevoked(true);
    drogon::orm::CoroMapper<drogon_model::notes_db::RefreshTokens> transMapper(trans);
    co_await transMapper.update(dbToken.value());

    std::string new_refresh_token_string = generateRefreshTokenString();
    drogon_model::notes_db::RefreshTokens new_refresh_token;
    new_refresh_token.setToken(new_refresh_token_string);
    new_refresh_token.setExpiresAt(trantor::Date::now().after(30 * 24 * 3600));
    new_refresh_token.setUserId(user_id);
    co_await transMapper.insert(std::move(new_refresh_token));

    Json::Value json;
    json["access_token"] = std::move(new_access_token);
    json["refresh_token"] = std::move(new_refresh_token_string);
    json["token_type"] = "Bearer";
    co_return ResponseBuilder::createSuccess("new tokens created",
                                             drogon::HttpStatusCode::k201Created, std::move(json));
  } catch (const drogon::orm::DrogonDbException& error) {
    trans->rollback();
    LOG_ERROR << "TOKEN ROTATION DB ERROR: " << error.base().what() << '\n';
    co_return ResponseBuilder::createError("token rotation db error",
                                           drogon::HttpStatusCode::k500InternalServerError);
  }
}

auto UserController::Me(const drogon::HttpRequestPtr request)
    -> drogon::Task<drogon::HttpResponsePtr> {
  int32_t user_id = request->attributes()->get<int32_t>("sub");
  auto db = drogon::app().getDbClient();
  if (!db) {
    LOG_ERROR << "DATABASE CONNECTION ERROR";
    co_return ResponseBuilder::createError("Database connection error",
                                           drogon::HttpStatusCode::k500InternalServerError);
  }
  drogon::orm::CoroMapper<drogon_model::notes_db::Users> usersMapper(db);
  try {

    drogon_model::notes_db::Users current_user = co_await usersMapper.findByPrimaryKey(user_id);
    co_return ResponseBuilder::createSuccess("Current user", drogon::HttpStatusCode::k200OK,
                                             std::move(current_user).toJson());
  } catch (drogon::orm::DrogonDbException& error) {
    LOG_ERROR << "DATABASE ERROR: " << error.base().what();
    co_return ResponseBuilder::createError("Database error",
                                           drogon::HttpStatusCode::k500InternalServerError);
  }
}

auto UserController::editPublicData(const drogon::HttpRequestPtr request)
    -> drogon ::Task<drogon::HttpResponsePtr> {
  auto user_id = request->attributes()->get<int32_t>("sub");
  auto json_ptr = request->getJsonObject();
  if (!json_ptr)
    co_return ResponseBuilder::createError("No json provided");
  auto errors = JsonValidate::validateJson(*json_ptr, JsonValidate::optional<std::string>("phone"),
                                           JsonValidate::optional<std::string>("username"),
                                           JsonValidate::optional<int32_t>("age"));
  if (!errors.empty())
    co_return ResponseBuilder::createError("Invalid json", drogon::HttpStatusCode::k400BadRequest,
                                           JsonBuilder::createErrorJsonByVector(std::move(errors)));
  auto db = drogon::app().getDbClient();
  if (!db) {
    LOG_ERROR << "DATABASE CONNECTION ERROR";
    co_return ResponseBuilder::createError("Database connection error",
                                           drogon::HttpStatusCode::k500InternalServerError);
  }
  drogon_model::notes_db::Users user;
  drogon::orm::CoroMapper<drogon_model::notes_db::Users> u_mapper(db);
  try {
    auto user = co_await u_mapper.findByPrimaryKey(user_id);
    if (json_ptr->isMember("username"))
      user.setUsername((*json_ptr)["username"].asString());
    if (json_ptr->isMember("age"))
      user.setAge((*json_ptr)["age"].asInt());
    if (json_ptr->isMember("phone"))
      user.setPhone((*json_ptr)["phone"].asString());
    user.setUpdatedAt(trantor::Date::now());
    co_await u_mapper.update(user);
    co_return ResponseBuilder::createSuccess("user edited", drogon::HttpStatusCode::k201Created,
                                             std::move(user).toJson());
  } catch (const drogon::orm::DrogonDbException& error) {
    LOG_ERROR << "DATABASE ERROR: " << error.base().what();
    std::string e = error.base().what();
    if (e.find("duplicate key") != std::string::npos)
      co_return ResponseBuilder::createError("Duplicate key", drogon::HttpStatusCode::k409Conflict);
    co_return ResponseBuilder::createError("Database error",
                                           drogon::HttpStatusCode::k500InternalServerError);
  }
}

// доделать!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
auto UserController::editPrivateData(const drogon::HttpRequestPtr request)
    -> drogon::Task<drogon::HttpResponsePtr> {
  auto user_id = request->attributes()->get<int32_t>("sub");
  auto json_ptr = request->getJsonObject();
  if (!json_ptr)
    co_return ResponseBuilder::createError("Invalid Json or no Json provided",
                                           drogon::HttpStatusCode::k400BadRequest);

  auto errors =
      JsonValidate::validateJson(*json_ptr, JsonValidate::optionalNotEmpty<std::string>("login"),
                                 JsonValidate::optionalNotEmpty<std::string>("email"),
                                 JsonValidate::optionalNotEmpty<std::string>("password"));
  if (!errors.empty())
    co_return ResponseBuilder::createError("Invalid json", drogon::HttpStatusCode::k400BadRequest,
                                           JsonBuilder::createErrorJsonByVector(std::move(errors)));
  auto current_loop = trantor::EventLoop::getEventLoopOfCurrentThread();
  auto db = drogon::app().getDbClient();
  if (!db) {
    LOG_ERROR << "DATABASE CONNECTION ERROR";
    co_return ResponseBuilder::createError("Database connection error",
                                           drogon::HttpStatusCode::k500InternalServerError);
  }
  drogon::orm::CoroMapper<drogon_model::notes_db::Users> u_mapper(db);
  try {
    auto current_user = co_await u_mapper.findByPrimaryKey(user_id);
    if (json_ptr->isMember("email"))
      current_user.setEmail((*json_ptr)["email"].asString());
    if (json_ptr->isMember("login"))
      current_user.setLogin((*json_ptr)["login"].asString());
    if (json_ptr->isMember("password")) {
      try {
        auto password_hash = co_await run_in_pool(
            getGlobalPool(), current_loop,
            [password = (*json_ptr)["password"].asString()]() -> std::optional<std::string> {
              return Hasher::hash(password);
            });
        if (password_hash.has_value())
          current_user.setPasswordHash(password_hash.value());
        else
          co_return ResponseBuilder::createError("Fatal error",
                                                 drogon::HttpStatusCode::k500InternalServerError);
      } catch (const std::exception& error) {
        LOG_ERROR << "THREADPOOL ERROR IN HASHER" << error.what();
        co_return ResponseBuilder::createError("Something went wrong",
                                               drogon::HttpStatusCode::k500InternalServerError);
      }
    }

    co_await u_mapper.update(current_user);
    co_return ResponseBuilder::createSuccess("email changed", drogon::HttpStatusCode::k201Created,
                                             current_user.toJson());
  } catch (drogon::orm::DrogonDbException& error) {
    LOG_ERROR << "DATABASE ERROR: " << error.base().what();
    std::string e = error.base().what();
    if (e.find("duplicate key") != std::string::npos)
      co_return ResponseBuilder::createError("Duplicate key", drogon::HttpStatusCode::k409Conflict);
    co_return ResponseBuilder::createError("Database error",
                                           drogon::HttpStatusCode::k500InternalServerError);
  }
}

auto UserController::signOutFromAllDevices(const drogon::HttpRequestPtr request)
    -> drogon::Task<drogon::HttpResponsePtr> {
  auto db = drogon::app().getDbClient();
  if (!db) {
    LOG_ERROR << "DATABASE CONNECTION ERROR";
    co_return ResponseBuilder::createError("Database connection error",
                                           drogon::HttpStatusCode::k500InternalServerError);
  }
  auto user_id = request->attributes()->get<int32_t>("sub");
  try {
    co_await db->execSqlCoro("update refresh_tokens "
                             "set is_revoked = true "
                             "where user_id = $1",
                             user_id);
    co_return ResponseBuilder::createSuccess("success log out", drogon::HttpStatusCode::k200OK);
  } catch (const drogon::orm::DrogonDbException& error) {
    LOG_ERROR << "DATABASE ERROR: " << error.base().what();
    co_return ResponseBuilder::createError("Database error",
                                           drogon::HttpStatusCode::k500InternalServerError);
  }
}