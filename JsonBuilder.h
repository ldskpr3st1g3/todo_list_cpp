#pragma once
#include <json/json.h>
#include <json/value.h>
#include <vector>

namespace JsonBuilder {
inline auto createErrorJsonByVector(std::vector<std::string>&& errors) -> Json::Value {
  Json::Value json;
  json["errors"] = Json::Value(Json::arrayValue);
  for (const auto& error : errors)
    json["errors"].append(error);
  return json;
}
} // namespace JsonBuilder