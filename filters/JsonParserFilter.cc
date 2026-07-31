#include "JsonParserFilter.h"
#include "JsonBuilder.h"
#include "JsonKeys.h"
#include "ResponseBuilder.h"
#include <drogon/HttpTypes.h>

auto JsonParserFilter::myFilter(const drogon::HttpRequestPtr& request)
    -> drogon::Task<std::optional<drogon::HttpResponsePtr>> {
  auto json_ptr = request->getJsonObject();
  std::vector<std::string> errors;
  for (auto it = json_ptr->begin(); it != json_ptr->end(); ++it) {
    if (!isValidKey(it.key().asString()))
      errors.push_back(std::format("key '{}' is not allowed", it.key().asString()));
  }
  if (errors.empty())
    co_return std::nullopt;
  co_return ResponseBuilder::createError(
      "Invalid json", drogon::HttpStatusCode::k400BadRequest,
      std::move(JsonBuilder::createErrorJsonByVector(std::move(errors))));
}
