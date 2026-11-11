#include "services/UserService.h"
#include "ErrorLogger.h"
#include "Exceptions.h"
#include "PoolAwaiter.h"
#include "SodiumHasher.h"
#include "Users.h"
#include "dto/UserDto.h"
#include <drogon/HttpTypes.h>
#include <drogon/orm/Exception.h>
#include <json/value.h>
#include <jwt-cpp/jwt.h>
#include <trantor/net/EventLoop.h>
#include <trantor/net/EventLoopThreadPool.h>

auto HashPassword(std::string&& password) -> drogon::Task<std::optional<std::string>> {
  try {
    auto opt =
        co_await run_in_pool(getGlobalPool(), trantor::EventLoop::getEventLoopOfCurrentThread(),
                             [password = std::move(password)]() { return Hasher::hash(password); });
    if (!opt)
      co_return std::nullopt;
    co_return *opt;
  } catch (const std::exception& e) {
    ErrorLogger::printThreadPoolError(__func__, e);
    co_return std::nullopt;
  }
}

auto UserService::getUserById(int pk)
    -> drogon::Task<std::optional<drogon_model::notes_db::Users>> {
  drogon_model::notes_db::Users user;
  try {
    co_return co_await user_repo_ptr->getById(pk);
  } catch (const drogon::orm::UnexpectedRows& e) {
    co_return std::nullopt;
  } catch (const drogon::orm::DrogonDbException& e) {
    LOG_ERROR << std::format("Database error in {}: ", __func__) << e.base().what();
    throw;
  }
}

auto UserService::deleteUserById(int pk) -> drogon::Task<int> {
  try {
    auto deletions = co_await user_repo_ptr->deleteById(pk);
    co_return deletions;
  } catch (const drogon::orm::DrogonDbException& e) {
    ErrorLogger::printDatabaseError(__func__, e);
    throw;
  }
}

auto UserService::authorizeUser(std::shared_ptr<Json::Value> payload)
    -> drogon::Task<std::optional<Json::Value>> {
  std::optional<drogon_model::notes_db::Users> user;
  auto& json = *payload;
  try {
    user = co_await user_repo_ptr->getUserByEmail(json["email"].asString());
    if (!user)
      co_return std::nullopt;
  } catch (const drogon::orm::DrogonDbException& e) {
    ErrorLogger::printDatabaseError(__func__, e);
    throw;
  }
  auto user_password = json["password"].asString();
  bool flag;
  auto password_hash = user->getValueOfPasswordHash();
  try {
    flag = co_await run_in_pool(
        getGlobalPool(), trantor::EventLoop::getEventLoopOfCurrentThread(),
        [password = std::move(user_password), password_hash = std::move(password_hash)]() {
          return Hasher::verify(password, password_hash);
        });
  } catch (const std::exception& e) {
    ErrorLogger::printThreadPoolError(__func__, e);
    throw ThreadPoolException(e.what());
  }
  if (!flag)
    co_return std::nullopt;
  try {
    co_return co_await token_service->generateTokens(user->getValueOfId());
  } catch (const drogon::orm::DrogonDbException& e) {
    ErrorLogger::printDatabaseError(__func__, e);
    throw;
  } catch (const std::exception& e) {
    ErrorLogger::printJwtError(__func__, e);
    throw JwtException(e.what());
  }
}

/*
auto UserService::registerUser(std::shared_ptr<Json::Value> payload) ->
drogon::Task<std::optional<UserDto>> { auto &j = *payload; try { auto check_user = co_await
user_repo_ptr->getUserByEmail(j["email"].asString()); if(check_user) co_return std::nullopt;
  }
  auto password = j["password"].asString();
  // вынести в отдельную функцию хэширование пароля
  std::string password_hash;
  try {
    auto opt = co_await
run_in_pool(getGlobalPool(),trantor::EventLoop::getEventLoopOfCurrentThread(),[password =
std::move(password)](){ return Hasher::hash(password);
    });
    if(opt) password_hash = opt.value();
    else {
      LOG_ERROR << std::format("Hasher error in {}: ", __func__);
    }
  } catch(const std::exception& e) {
    ErrorLogger::printThreadPoolError(__func__, e);
  }
  //
  drogon_model::notes_db::Users user;
  user.updateByMasqueradedJson(j, {"", "username", "email", "", "", "", "login", "", ""});
  user.setPasswordHash(std::move(password_hash));
  try {
    co_await user_repo_ptr->insert(user);
  } catch(const drogon::orm::DrogonDbException& e) {
    ErrorLogger::printDatabaseError(__func__, e);
    if(std::string(e.base().what()).find("duplicate key") != std::string::npos)
      co_return std::nullopt;
    throw;
  }
  co_return UserDto(user);
}
*/

auto UserService::RegisterUser(std::shared_ptr<Json::Value> payload)
    -> drogon::Task<std::pair<std::optional<UserDto>, drogon::HttpStatusCode>> {
  auto& j = *payload;
  try {
    auto first_check = co_await user_repo_ptr->getUserByEmail(j["email"].asString());
    auto second_check = co_await user_repo_ptr->getUserByLogin(j["login"].asString());
    if (first_check || second_check)
      co_return std::make_pair(std::nullopt, drogon::HttpStatusCode::k409Conflict);
  } catch (const drogon::orm::DrogonDbException& e) {
    ErrorLogger::printDatabaseError(__func__, e);
    co_return std::make_pair(std::nullopt, drogon::HttpStatusCode::k500InternalServerError);
  }
  auto password = j["password"].asString();
  auto password_hash = co_await HashPassword(std::move(password));
  if (!password_hash)
    co_return std::make_pair(std::nullopt, drogon::HttpStatusCode::k500InternalServerError);
  drogon_model::notes_db::Users user;
  user.updateByMasqueradedJson(j, {"", "username", "email", "", "", "", "login", "", ""});
  user.setPasswordHash(std::move(*password_hash));
  try {
    user = co_await user_repo_ptr->insert(std::move(user));
  } catch (const drogon::orm::DrogonDbException& e) {
    ErrorLogger::printDatabaseError(__func__, e);
    std::string error = e.base().what();
    if (error.find("duplicate key") != std::string::npos ||
        error.find("23505") != std::string::npos)
      co_return std::make_pair(std::nullopt, drogon::HttpStatusCode::k409Conflict);
    co_return std::make_pair(std::nullopt, drogon::HttpStatusCode::k500InternalServerError);
  }
  co_return std::make_pair(UserDto(user), drogon::HttpStatusCode::k201Created);
}
