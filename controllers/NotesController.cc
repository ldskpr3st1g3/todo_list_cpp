#include "NotesController.h"
#include "JsonBuilder.h"
#include "JsonValidate.h"
#include "RefreshTokens.h"
#include "ResponseBuilder.h"
#include "Users.h"
#include "UsersNotes.h"
#include "models/Notes.h"
#include <cstdint>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <drogon/HttpTypes.h>
#include <drogon/orm/CoroMapper.h>
#include <drogon/orm/Criteria.h>
#include <drogon/orm/Exception.h>
#include <json/value.h>

bool RoleChecker(std::string&& role, std::initializer_list<std::string> roles) {
  return std::find(std::begin(roles), std::end(roles), role) < std::end(roles);
}

auto NoteController::createNewNote(const drogon::HttpRequestPtr request)
    -> drogon::Task<drogon::HttpResponsePtr> {
  auto json_ptr = request->getJsonObject();
  if (!json_ptr)
    co_return ResponseBuilder::createError("No json provided");
  std::vector<std::string> errors =
      JsonValidate::validateJson(*json_ptr, JsonValidate::required<std::string>("title"),
                                 JsonValidate::optional<std::string>("content"));
  if (!errors.empty()) {
    co_return ResponseBuilder::createError("Invalid json", drogon::k400BadRequest,
                                           JsonBuilder::createErrorJsonByVector(std::move(errors)));
  }

  auto db = drogon::app().getDbClient();
  if (!db) {
    LOG_ERROR << "DATABASE CONNECTION ERROR";
    co_return ResponseBuilder::createError("Database connection error",
                                           drogon::HttpStatusCode::k500InternalServerError);
  }
  auto user_id = request->attributes()->get<int32_t>("sub");
  drogon_model::notes_db::Notes new_note;
  Json::Value json_for_creation;
  json_for_creation["title"] = (*json_ptr)["title"].asString();
  json_for_creation["content"] = (*json_ptr)["content"].asString();
  new_note.updateByJson(std::move(json_for_creation));
  auto trans = co_await db->newTransactionCoro();
  try {
    drogon::orm::CoroMapper<drogon_model::notes_db::Notes> notesMapper(trans);
    auto current_note = co_await notesMapper.insert(std::move(new_note));
    drogon_model::notes_db::UsersNotes new_conjuction;
    new_conjuction.setNoteId(*current_note.getId());
    new_conjuction.setUserId(user_id);
    drogon::orm::CoroMapper<drogon_model::notes_db::UsersNotes> conjMapper(trans);
    co_await conjMapper.insert(std::move(new_conjuction));
    co_return ResponseBuilder::createSuccess("Note created", drogon::HttpStatusCode::k201Created,
                                             std::move(current_note).toJson());

  } catch (const drogon::orm::DrogonDbException& error) {
    trans->rollback();
    LOG_ERROR << "DATABASE ERROR" << error.base().what() << '\n';
    co_return ResponseBuilder::createError("Database error",
                                           drogon::HttpStatusCode::k500InternalServerError);
  }
}

auto NoteController::getAllHeadsOfNotes(const drogon::HttpRequestPtr request)
    -> drogon::Task<drogon::HttpResponsePtr> {
  int32_t user_id = request->attributes()->get<int32_t>("sub");
  auto db = drogon::app().getDbClient();
  if (!db) {
    LOG_ERROR << "DATABASE CONNECTION ERROR";
    co_return ResponseBuilder::createError("Database connection error",
                                           drogon::HttpStatusCode::k500InternalServerError);
  }
  try {
    auto notesInfo = co_await db->execSqlCoro("select n.id, n.title, n.updated_at from notes n "
                                              "join users_notes un on n.id  = un.note_id "
                                              "where un.user_id = $1 order by n.updated_at desc",
                                              user_id);
    Json::Value note_json(Json::arrayValue);
    for (const auto& row : notesInfo) {
      Json::Value json;
      json["id"] = row["id"].as<int32_t>();
      json["title"] = row["title"].as<std::string>();
      note_json.append(std::move(json));
    }
    co_return ResponseBuilder::createSuccess("Notes list", drogon::HttpStatusCode::k200OK,
                                             note_json);
  } catch (const drogon::orm::DrogonDbException& error) {
    LOG_ERROR << "DATABASE ERROR: " << error.base().what();
    co_return ResponseBuilder::createError("Database error",
                                           drogon::HttpStatusCode::k500InternalServerError);
  }
}

auto NoteController::getNoteById(const drogon::HttpRequestPtr request, int32_t note_id)
    -> drogon::Task<drogon::HttpResponsePtr> {
  int32_t user_id = request->attributes()->get<int32_t>("sub");
  auto db = drogon::app().getDbClient();
  if (!db) {
    LOG_ERROR << "DATABASE CONNECTION ERROR";
    co_return ResponseBuilder::createError("Database connection error",
                                           drogon::HttpStatusCode::k500InternalServerError);
  }
  try {
    auto needed_note =
        co_await db->execSqlCoro("select n.* , un.role from notes n "
                                 "join users_notes un on un.note_id = n.id and un.user_id = $1 "
                                 "where n.id = $2",
                                 user_id, note_id);
    if (needed_note.empty())
      co_return ResponseBuilder::createError("something went wrong",
                                             drogon::HttpStatusCode::k500InternalServerError);
    auto row = needed_note[0];
    Json::Value json;
    json["id"] = row["id"].as<int32_t>();
    json["title"] = row["title"].as<std::string>();
    json["content"] = row["content"].as<std::string>();
    json["created_at"] = row["created_at"].as<std::string>();
    json["updated_at"] = row["updated_at"].as<std::string>();
    json["role"] = row["role"].as<std::string>();
    co_return ResponseBuilder::createSuccess("Note details", drogon::HttpStatusCode::k200OK, json);

  } catch (const drogon::orm::DrogonDbException& error) {
    LOG_ERROR << "DATABASE ERROR: " << error.base().what();
    co_return ResponseBuilder::createError("Database error",
                                           drogon::HttpStatusCode::k500InternalServerError);
  }
}

auto NoteController::deleteNoteById(const drogon::HttpRequestPtr request, int32_t note_id)
    -> drogon::Task<drogon::HttpResponsePtr> {

  auto user_role = request->attributes()->get<std::string>("role");
  if (!RoleChecker(std::move(user_role), {"owner"}))
    co_return ResponseBuilder::createError("Access denied", drogon::HttpStatusCode::k403Forbidden);
  auto db = drogon::app().getDbClient();
  if (!db) {
    LOG_ERROR << "DATABASE CONNECTION ERROR";
    co_return ResponseBuilder::createError("Database connection error",
                                           drogon::HttpStatusCode::k500InternalServerError);
  }
  drogon::orm::CoroMapper<drogon_model::notes_db::Notes> note_mapper(db);
  try {
    auto del = co_await note_mapper.deleteByPrimaryKey(note_id);
    co_return del ? ResponseBuilder::createSuccess("Note deleted", drogon::HttpStatusCode::k200OK)
                  : ResponseBuilder::createSuccess("Note with this id does not exist",
                                                   drogon::HttpStatusCode::k200OK);

  } catch (const drogon::orm::DrogonDbException& error) {
    LOG_ERROR << "DATABASE ERROR: " << error.base().what();
    co_return ResponseBuilder::createError("Database error",
                                           drogon::HttpStatusCode::k500InternalServerError);
  }
}

auto NoteController::editNoteById(const drogon::HttpRequestPtr request, int32_t note_id)
    -> drogon::Task<drogon::HttpResponsePtr> {
  auto user_role = request->attributes()->get<std::string>("role");
  if (!RoleChecker(std::move(user_role), {"editor", "owner"}))
    co_return ResponseBuilder::createError("Access denied", drogon::HttpStatusCode::k403Forbidden);
  auto json_ptr = request->getJsonObject();
  if (!json_ptr)
    co_return ResponseBuilder::createError("No json provided");
  auto errors =
      JsonValidate::validateJson(*json_ptr, JsonValidate::optionalNotEmpty<std::string>("title"),
                                 JsonValidate::optional<std::string>("content"));
  if (!errors.empty())
    co_return ResponseBuilder::createError("Invalid json", drogon::HttpStatusCode::k400BadRequest,
                                           JsonBuilder::createErrorJsonByVector(std::move(errors)));
  auto db = drogon::app().getDbClient();
  if (!db) {
    LOG_ERROR << "DATABASE CONNECTION ERROR";
    co_return ResponseBuilder::createError("Database connection error",
                                           drogon::HttpStatusCode::k500InternalServerError);
  }
  drogon::orm::CoroMapper<drogon_model::notes_db::Notes> mapper(db);
  try {
    auto user_id = request->attributes()->get<int32_t>("sub");

    auto note = co_await mapper.findByPrimaryKey(note_id);
    note.updateByMasqueradedJson(*json_ptr, {"", "title", "content", "", ""});

    note.setUpdatedAt(trantor::Date::now());

    co_await mapper.update(note);
    co_return ResponseBuilder::createSuccess("Note edited", drogon::HttpStatusCode::k200OK,
                                             std::move(note.toJson()));
  } catch (const drogon::orm::UnexpectedRows&) {
    LOG_ERROR << "Note not found" << '\n';
    co_return ResponseBuilder::createError("Note not found", drogon::HttpStatusCode::k404NotFound);
  } catch (const drogon::orm::DrogonDbException& exp) {
    LOG_ERROR << "DATABASE ERROR" << exp.base().what() << '\n';
    co_return ResponseBuilder::createError("Database Error",
                                           drogon::HttpStatusCode::k500InternalServerError);
  }
}

auto NoteController::addRoleByUserId(const drogon::HttpRequestPtr request, int32_t note_id)
    -> drogon::Task<drogon::HttpResponsePtr> {
  auto user_role = request->attributes()->get<std::string>("role");
  if (!RoleChecker(std::move(user_role), {"owner"}))
    co_return ResponseBuilder::createError("Access denied", drogon::HttpStatusCode::k403Forbidden);

  auto json_ptr = request->getJsonObject();
  if (!json_ptr)
    co_return ResponseBuilder::createError("No json provided");
  auto errors = JsonValidate::validateJson(*json_ptr, JsonValidate::required<std::string>("login"),
                                           JsonValidate::required<std::string>("role"));
  if (!errors.empty())
    co_return ResponseBuilder::createError("Invalid json", drogon::HttpStatusCode::k400BadRequest,
                                           JsonBuilder::createErrorJsonByVector(std::move(errors)));
  auto db = drogon::app().getDbClient();
  if (!db) {
    LOG_ERROR << "DATABASE CONNECTION ERROR";
    co_return ResponseBuilder::createError("Database connection error",
                                           drogon::HttpStatusCode::k500InternalServerError);
  }
  drogon::orm::CoroMapper<drogon_model::notes_db::UsersNotes> un_mapper(db);
  drogon::orm::CoroMapper<drogon_model::notes_db::Users> u_mapper(db);
  auto new_role = (*json_ptr)["role"].asString();
  try {

    auto user = co_await u_mapper.findBy(
        drogon::orm::Criteria(drogon_model::notes_db::Users::Cols::_login,
                              drogon::orm::CompareOperator::EQ, (*json_ptr)["login"].asString()));
    if (user.empty())
      co_return ResponseBuilder::createError("User not found",
                                             drogon::HttpStatusCode::k404NotFound);
    auto user_id = *user.front().getId();

    auto conjuction = co_await un_mapper.findBy(
        drogon::orm::Criteria(drogon_model::notes_db::UsersNotes::Cols::_note_id,
                              drogon::orm::CompareOperator::EQ, note_id) &&
        drogon::orm::Criteria(drogon_model::notes_db::UsersNotes::Cols::_user_id,
                              drogon::orm::CompareOperator::EQ, user_id));
    if (conjuction.empty()) {
      drogon_model::notes_db::UsersNotes new_conjuction;
      new_conjuction.setRole(std::move(new_role));
      new_conjuction.setNoteId(note_id);
      new_conjuction.setUserId(user_id);
      auto db_conjuction = co_await un_mapper.insert(std::move(new_conjuction));
      co_return ResponseBuilder::createSuccess("Role added", drogon::HttpStatusCode::k201Created,
                                               db_conjuction.toJson());
    } else {
      auto actual_conjuction = conjuction.front();
      actual_conjuction.setRole(std::move(new_role));
      actual_conjuction.setGrantedAt(trantor::Date::now());
      co_await un_mapper.update(actual_conjuction);
      co_return ResponseBuilder::createSuccess("Role added", drogon::HttpStatusCode::k201Created,
                                               actual_conjuction.toJson());
    }
  } catch (drogon::orm::DrogonDbException& error) {
    LOG_ERROR << "DATABASE ERROR: " << error.base().what();
    co_return ResponseBuilder::createError("Database error",
                                           drogon::HttpStatusCode::k500InternalServerError);
  }
}
