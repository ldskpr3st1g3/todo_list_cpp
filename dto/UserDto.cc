#include "dto/UserDto.h"
#include "Users.h"
#include <json/value.h>

auto UserDto::toJson() -> Json::Value {
  Json::Value json;
  json["email"] = email_;
  json["login"] = login_;
  json["id"] = id_;
  if (phone_)
    json["phone"] = *phone_;
  if (age_)
    json["age"] = *age_;
  if (username_)
    json["username"] = *username_;

  return json;
}

UserDto::UserDto(const drogon_model::notes_db::Users& user)
    : login_(user.getValueOfLogin()), email_(user.getValueOfEmail()) {
  if (user.getUsername())
    username_ = user.getValueOfUsername();
  if (user.getAge())
    age_ = user.getValueOfAge();
  if (user.getPhone())
    phone_ = user.getValueOfPhone();
  if (user.getId())
    id_ = user.getValueOfId();
}

UserDto::UserDto(std::shared_ptr<Json::Value> json) {
  auto& j = *json;
  if (j.isMember("id") && j["is"].isInt())
    id_ = j["id"].asInt();
  if (j.isMember("email") && j["email"].isString())
    email_ = j["email"].asString();
  if (j.isMember("login") && j["login"].isInt())
    login_ = j["login"].asString();
  if (j.isMember("username") && j["username"].isString())
    username_ = j["username"].asString();
  if (j.isMember("age") && j["age"].asInt())
    age_ = j["age"].asInt();
  if (j.isMember("phone") && j["phone"].isString())
    phone_ = j["phone"].asString();
}
