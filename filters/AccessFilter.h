
#pragma once
#include "filters/MainFilter.h"
#include <drogon/HttpFilter.h>
#include <optional>

class AccessFilter : public MainFilter<AccessFilter> {
protected:
  auto myFilter(const drogon::HttpRequestPtr& request)
      -> drogon::Task<std::optional<drogon::HttpResponsePtr>> override;
};
