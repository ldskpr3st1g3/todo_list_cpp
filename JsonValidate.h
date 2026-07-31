#pragma once
#include <format>
#include <json/json.h>

namespace JsonValidate {

template <typename T> inline auto checkJsonType(const Json::Value& pjson) -> bool {
  if (pjson.isNull())
    return false;
  if constexpr (std::is_same_v<T, int>)
    return pjson.isInt();
  else if constexpr (std::is_same_v<T, std::string>)
    return pjson.isString();
  else if constexpr (std::is_same_v<T, bool>)
    return pjson.isBool();
  return false;
}

template <typename T> struct FieldRule {

  std::string_view name;
  bool is_required;
  bool is_empty;
};

template <typename T> inline auto required(std::string_view field_name) -> FieldRule<T> {
  return {field_name, true, false};
}
template <typename T> inline auto optional(std::string_view field_name) -> FieldRule<T> {
  return {field_name, false, true};
}
template <typename T> inline auto optionalNotEmpty(std::string_view field_name) -> FieldRule<T> {
  return {field_name, false, false};
}

template <typename T>
inline void RuleCheck(const Json::Value& pjson, FieldRule<T>& rule,
                      std::vector<std::string>& errors) {
  auto field_name = rule.name;
  if (!pjson.isMember(field_name)) {
    if (rule.is_required) {
      errors.push_back(std::format("Missing required field: '{}'", field_name));
    }
    return;
  }
  if (!checkJsonType<T>(pjson[field_name]))
    errors.push_back(std::format("Invalid type for field '{}'", field_name));
  if (!rule.is_empty && std::is_same_v<T, std::string> && pjson[field_name].asString().empty())
    errors.push_back(std::format("Field '{}' can't be empty", field_name));
}

template <typename... Rules>
inline auto validateJson(const Json::Value& pjson, Rules... rules) -> std::vector<std::string> {
  std::vector<std::string> errors;
  (RuleCheck(pjson, rules, errors), ...);
  return errors;
}

} // namespace JsonValidate