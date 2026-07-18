#pragma once

#include <drogon/HttpFilter.h>
#include <drogon/HttpTypes.h>
#include <drogon/utils/coroutine.h>
#include <optional>

template <typename T> class MainFilter : public drogon::HttpFilter<T> {
protected:
  virtual auto myFilter(const drogon::HttpRequestPtr& request)
      -> drogon::Task<std::optional<drogon::HttpResponsePtr>> = 0;

public:
  auto doFilter(const drogon::HttpRequestPtr& request, drogon::FilterCallback&& fcb,
                drogon::FilterChainCallback&& fccb) -> void override final {
    drogon::async_run(
        [request, fcb = std::move(fcb), fccb = std::move(fccb), this]() -> drogon::Task<void> {
          try {
            std::optional<drogon::HttpResponsePtr> response = co_await myFilter(request);
            if (response.has_value()) {
              fcb(response.value());
            } else
              fccb();
          } catch (...) {
            auto response = drogon::HttpResponse::newHttpJsonResponse({});
            response->setStatusCode(drogon::HttpStatusCode::k500InternalServerError);
            fcb(response);
          }
        });
  }
};