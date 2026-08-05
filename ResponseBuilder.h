#pragma once

#include <drogon/HttpResponse.h>
#include <drogon/orm/DbClient.h>
#include <optional>
#include <string_view>
namespace ResponseBuilder {
inline auto createSuccess(std::string_view message,
                          drogon::HttpStatusCode code = drogon::HttpStatusCode::k200OK,
                          std::optional<Json::Value> js = std::nullopt) -> drogon::HttpResponsePtr {
  Json::Value json;
  json["status"] = "Success";
  json["message"] = message;
  if (js.has_value()) {
    json["data"] = std::move(js).value();
  }
  auto response = drogon::HttpResponse::newHttpJsonResponse(json);
  response->setStatusCode(code);
  return response;
}

inline auto createError(std::string_view message,
                        drogon::HttpStatusCode&& code = drogon::HttpStatusCode::k400BadRequest,
                        std::optional<Json::Value> js = std::nullopt) -> drogon::HttpResponsePtr {
  Json::Value json;
  json["status"] = "Error";
  json["message"] = message;
  if (js.has_value()) {
    json["errors"] = std::move(js).value()["errors"];
  }
  auto response = drogon::HttpResponse::newHttpJsonResponse(json);
  response->setStatusCode(code);
  return response;
}

} // namespace ResponseBuilder