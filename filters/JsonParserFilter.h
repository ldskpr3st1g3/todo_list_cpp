
#pragma once

#include "MainFilter.h"
#include <drogon/HttpFilter.h>
#include <json/json.h>

class JsonParserFilter : public MainFilter<JsonParserFilter> {
protected:
  auto myFilter(const drogon::HttpRequestPtr& request)
      -> drogon::Task<std::optional<drogon::HttpResponsePtr>> override;
};
