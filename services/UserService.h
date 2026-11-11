#pragma once
#include "Users.h"
#include "dto/UserDto.h"
#include "repositories/UserRepository.h"
#include "services/TokenService.h"
#include <drogon/drogon.h>
#include <json/value.h>

class UserService {
  std::shared_ptr<UserRepository> user_repo_ptr;
  std::shared_ptr<TokenService> token_service;

public:
  explicit UserService(std::shared_ptr<UserRepository> urp, std::shared_ptr<TokenService> ts)
      : user_repo_ptr(std::move(urp)), token_service(std::move(ts)) {}

  auto deleteUserById(int pk) -> drogon::Task<int>;
  auto authorizeUser(std::shared_ptr<Json::Value> payload)
      -> drogon::Task<std::optional<Json::Value>>;
  auto registerUser(std::shared_ptr<Json::Value> payload) -> drogon::Task<std::optional<UserDto>>;
  auto getUserById(int pk) -> drogon::Task<std::optional<drogon_model::notes_db::Users>>;
  auto RegisterUser(std::shared_ptr<Json::Value> payload)
      -> drogon::Task<std::pair<std::optional<UserDto>, drogon::HttpStatusCode>>;
};