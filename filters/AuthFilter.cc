#include "AuthFilter.h"
#include "ResponseBuilder.h"
#include <drogon/drogon.h>
#include <jwt-cpp/jwt.h>

auto AuthFilter::myFilter(const drogon::HttpRequestPtr& request)
    -> drogon::Task<std::optional<drogon::HttpResponsePtr>> {
  auto param = request->getHeader("Authorization");
  if (param.empty() || param.find("Bearer ") != 0)
    co_return ResponseBuilder::createError("Missing or invalid Authorization header");
  auto token = param.substr(7);
  try {

    auto decoded = jwt::decode(token);
    int32_t user_id = stoi(decoded.get_payload_claim("sub").as_string());

    auto verifier = jwt::verify()
                        .allow_algorithm(jwt::algorithm::hs256{
                            drogon::app().getCustomConfig()["jwt"]["secret_key"].asString()})
                        .with_issuer("DEFECT");

    verifier.verify(std::move(decoded));
    request->attributes()->insert("sub", user_id);
    co_return std::nullopt;
  } catch (jwt::error::token_verification_exception& error) {
    LOG_ERROR << "JWT ERROR: " << error.what();
    co_return ResponseBuilder::createError("Expired or invalid jwt",
                                           drogon::HttpStatusCode::k401Unauthorized);

  } catch (const std::exception& error) {
    LOG_ERROR << "JWT ERROR: " << error.what();
    co_return ResponseBuilder::createError("Malformed jwt",
                                           drogon::HttpStatusCode::k401Unauthorized);
  }
}