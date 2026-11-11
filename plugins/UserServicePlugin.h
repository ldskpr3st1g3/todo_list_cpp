#pragma once
#include "repositories/UserRepository.h"
#include "services/UserService.h"
#include <drogon/drogon.h>
#include <drogon/plugins/Plugin.h>
#include <json/value.h>
#include <plugins/TokenServicePlugin.h>

class UserServicePlugin : public drogon::Plugin<UserServicePlugin> {
  std::shared_ptr<UserService> user_service_;

public:
  void initAndStart(const Json::Value& config) override {
    auto db = drogon::app().getDbClient();
    if (!db) {
      LOG_FATAL << "Database client is not initialized";
      exit(1);
    }

    user_service_ =
        std::make_shared<UserService>(std::make_shared<UserRepository>(db),
                                      drogon::app().getPlugin<TokenServicePlugin>()->getService());
    LOG_INFO << "UserServicePlugin initialized";
  }

  void shutdown() override { LOG_INFO << "UserServicePlugin shutdown"; }

  std::shared_ptr<UserService> getService() const { return user_service_; }
};