#pragma once
#include "repositories/RefreshRepository.h"
#include "services/TokenService.h"
#include <drogon/drogon.h>
#include <drogon/plugins/Plugin.h>
#include <json/value.h>

class TokenServicePlugin : public drogon::Plugin<TokenServicePlugin> {
  std::shared_ptr<TokenService> token_service_;

public:
  void initAndStart(const Json::Value& config) override {
    auto db = drogon::app().getDbClient();
    if (!db) {
      LOG_FATAL << "Database client is not initialized";
      exit(1);
    }
    token_service_ = std::make_shared<TokenService>(std::make_shared<RefreshRepository>(db));
    LOG_INFO << "UserServicePlugin initialized";
  }

  void shutdown() override { LOG_INFO << "UserServicePlugin shutdown"; }

  std::shared_ptr<TokenService> getService() const { return token_service_; }
};