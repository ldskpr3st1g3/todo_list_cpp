#pragma once
#include "Users.h"
#include <json/json.h>
#include <optional>
#include <string>

class UserDto {
private:
  int32_t id_;
  std::string login_;
  std::string email_;
  std::optional<int32_t> age_;
  std::optional<std::string> phone_;
  std::optional<std::string> username_;

public:
  auto toJson() -> Json::Value;
  explicit UserDto(std::shared_ptr<Json::Value> json);
  explicit UserDto(const drogon_model::notes_db::Users& user);
};
