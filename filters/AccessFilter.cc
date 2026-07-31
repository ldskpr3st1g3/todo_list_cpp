#include "AccessFilter.h"
#include "ResponseBuilder.h"
#include "UsersNotes.h"
#include <drogon/HttpTypes.h>
#include <drogon/drogon.h>
#include <drogon/utils/coroutine.h>
#include <string>

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
  auto db = drogon::app().getDbClient();
  drogon::orm::CoroMapper<drogon_model::notes_db::UsersNotes> un_mapper(db);
  if (!db) {
    LOG_ERROR << "DATABASE CONNECTION ERROR";
    co_return ResponseBuilder::createError("Database connection error",
                                           drogon::HttpStatusCode::k500InternalServerError);
  }
  try {
    auto conjuction = co_await un_mapper.findBy(
        drogon::orm::Criteria(drogon_model::notes_db::UsersNotes::Cols::_note_id,
                              drogon::orm::CompareOperator::EQ, note_id) &&
        drogon::orm::Criteria(drogon_model::notes_db::UsersNotes::Cols::_user_id,
                              drogon::orm::CompareOperator::EQ, user_id));
    if (conjuction.empty())
      co_return ResponseBuilder::createError("Access denied",
                                             drogon::HttpStatusCode::k403Forbidden);

    auto role = *conjuction[0].getRole();
    request->attributes()->insert("role", role);

    co_return std::nullopt;
  } catch (const drogon::orm::DrogonDbException& error) {
    LOG_ERROR << "DATABASE ERROR: " << error.base().what();
    co_return ResponseBuilder::createError("Database error",
                                           drogon::HttpStatusCode::k500InternalServerError);
  }
}
