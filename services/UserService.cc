#include "services/UserService.h"
#include "ErrorLogger.h"
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

auto VerifyPassword(std::string&& password, std::string&& password_hash) -> drogon::Task<bool> {
  bool flag = 0;
  try {
    flag = co_await run_in_pool(getGlobalPool(),trantor::EventLoop::getEventLoopOfCurrentThread(), [password = std::move(password), password_hash = std::move(password_hash)](){
    return Hasher::verify(password, password_hash);
  });
    if(!flag) co_return false;
  }catch(const std::exception& e) {
    ErrorLogger::printThreadPoolError(__func__, e);
    co_return false;
  }
  co_return true;
}

auto UserService::AuthorizeUser(std::shared_ptr<Json::Value> payload) -> drogon::Task<std::pair<std::optional<Json::Value>, drogon::HttpStatusCode>> {
  auto &j = *payload;
  std::string user_password;
  int32_t user_id;
  try {
    auto user = co_await user_repo_ptr->getUserByEmail(j["email"].asString());
    if(!user) co_return std::make_pair(std::nullopt, drogon::HttpStatusCode::k401Unauthorized);
    user_password = user->getValueOfPasswordHash();
    user_id = user->getValueOfId();
  } catch(const drogon::orm::DrogonDbException& e) {
    ErrorLogger::printDatabaseError(__func__, e);
    co_return std::make_pair(std::nullopt, drogon::HttpStatusCode::k500InternalServerError);
  }
  auto input_password = j["password"].asString();
  auto flag = co_await VerifyPassword(std::move(input_password), std::move(user_password));
  if(!flag) co_return std::make_pair(std::nullopt, drogon::HttpStatusCode::k401Unauthorized);
  auto output = co_await token_service->generateTokens(user_id);
  if(output) 
    co_return std::make_pair(*output, drogon::HttpStatusCode::k200OK);
  else co_return std::make_pair(std::nullopt, drogon::HttpStatusCode::k500InternalServerError);
}

auto UserService::GetUserById(int pk) -> drogon::Task<std::pair<std::optional<drogon_model::notes_db::Users>, drogon::HttpStatusCode>> {
  drogon_model::notes_db::Users user;
  try {
    auto deletions = co_await user_repo_ptr->getById(pk);
    co_return std::make_pair(deletions, drogon::HttpStatusCode::k200OK);
  } catch(const drogon::orm::UnexpectedRows& e) {
    co_return std::make_pair(std::nullopt, drogon::HttpStatusCode::k401Unauthorized);
  } catch(const drogon::orm::DrogonDbException& e)  {
    co_return std::make_pair(std::nullopt, drogon::HttpStatusCode::k500InternalServerError);
  }
}

auto UserService::DeleteUserById(int32_t pk) -> drogon::Task<std::optional<int32_t>> {
  try {
    co_return co_await user_repo_ptr->deleteById(pk);
  } catch(const drogon::orm::DrogonDbException& e) {
    ErrorLogger::printDatabaseError(__func__, e);
    co_return std::nullopt;
  }
}


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


