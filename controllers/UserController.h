#pragma once
#include <drogon/HttpController.h>
#include <drogon/HttpResponse.h>
#include <drogon/HttpTypes.h>

class UserController : public drogon::HttpController<UserController> {
public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(UserController::Registration, "/api/v1/users/registr", drogon::Post);
  ADD_METHOD_TO(UserController::Authorization, "/api/v1/users/login", drogon::Post);
  ADD_METHOD_TO(UserController::Refresh, "/api/v1/users/refresh", drogon::Post);
  ADD_METHOD_TO(UserController::Me, "api/v1/users/me", drogon::Get, "AuthFilter");
  ADD_METHOD_TO(UserController::editPublicData, "/api/v1/users/me/public", drogon::Patch,
                "AuthFilter");
  ADD_METHOD_TO(UserController::editPrivateData, "/api/v1/users/me/private", drogon::Patch,
                "AuthFilter");
  ADD_METHOD_TO(UserController::signOutFromAllDevices, "/api/v1/users/me/logout", drogon::Patch,
                "AuthFilter");

  METHOD_LIST_END

  auto Authorization(const drogon::HttpRequestPtr request) -> drogon::Task<drogon::HttpResponsePtr>;
  auto Registration(const drogon::HttpRequestPtr request) -> drogon::Task<drogon::HttpResponsePtr>;
  auto Refresh(const drogon::HttpRequestPtr request) -> drogon::Task<drogon::HttpResponsePtr>;
  auto Me(const drogon::HttpRequestPtr request) -> drogon::Task<drogon::HttpResponsePtr>;
  // new tasks
  auto editPublicData(const drogon::HttpRequestPtr request)
      -> drogon::Task<drogon::HttpResponsePtr>;
  auto signOutFromAllDevices(const drogon::HttpRequestPtr request)
      -> drogon::Task<drogon::HttpResponsePtr>;
  auto editPrivateData(const drogon::HttpRequestPtr request)
      -> drogon::Task<drogon::HttpResponsePtr>;
};
