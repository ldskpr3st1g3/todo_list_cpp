#pragma once
#include <json/json.h>

namespace JsonValidation {

inline auto myValidate(const Json::Value& pjson, std::initializer_list<std::string>&& heads) {
  for (const auto& head : heads) {
    if (!pjson.isMember(head))
      return false;
  }
  return true;
}
} // namespace JsonValidation