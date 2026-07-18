
#pragma once
#include "MainFilter.h"
#include <drogon/HttpFilter.h>

class AuthFilter : public MainFilter<AuthFilter> {
protected:
  auto myFilter(const drogon::HttpRequestPtr& request)
      -> drogon::Task<std::optional<drogon::HttpResponsePtr>> override;
};
