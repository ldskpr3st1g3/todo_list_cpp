#include "AccessFilter.h"
#include "ResponseBuilder.h"
#include <drogon/HttpTypes.h>
#include <drogon/drogon.h>
#include <drogon/utils/coroutine.h>

auto AccessFilter::myFilter(const drogon::HttpRequestPtr& request)
    -> drogon::Task<std::optional<drogon::HttpResponsePtr>> {
  auto user_id = request->attributes()->get<int32_t>("sub");
  const auto& path_params = request->getRoutingParameters();
  if (path_params.empty()) {
    LOG_ERROR << "AccessFilter: No path parameters found in request.";
    co_return ResponseBuilder::createError("Bad request", drogon::HttpStatusCode::k400BadRequest);
  }
  int32_t note_id = 0;
  try {
    note_id = std::stoi(path_params[0]);
  } catch (const std::exception& error) {
    LOG_ERROR << "AccessFilter: INT ERROR (invalid id format): " << error.what();
    co_return ResponseBuilder::createError("Invalid note ID format",
                                           drogon::HttpStatusCode::k400BadRequest);
  }

  try {
    auto db = drogon::app().getDbClient();
    auto access = co_await db->execSqlCoro(
        "select * from users_notes where note_id = $1 and user_id = $2", note_id, user_id);
    if (access.empty())
      co_return ResponseBuilder::createError("Access denied",
                                             drogon::HttpStatusCode::k403Forbidden);
    co_return std::nullopt;
  } catch (const drogon::orm::DrogonDbException& error) {
    LOG_ERROR << "DATABASE ERROR: " << error.base().what();
    co_return ResponseBuilder::createError("Database error",
                                           drogon::HttpStatusCode::k500InternalServerError);
  }
}
