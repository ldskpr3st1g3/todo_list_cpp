#include "NotesController.h"
#include "JsonValidate.h"
#include "RefreshTokens.h"
#include "ResponseBuilder.h"
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
#include <pthread.h>

auto NoteController::createNewNote(const drogon::HttpRequestPtr request)
    -> drogon::Task<drogon::HttpResponsePtr> {
  auto json_ptr = request->getJsonObject();
  if (!json_ptr ||
      !JsonValidation::myValidate(*json_ptr,
                                  {
                                      "title",
                                      "content",
                                  }) ||
      (*json_ptr)["title"].asString().empty())
    co_return ResponseBuilder::createError("Invalid JSON or no JSON provided");
  auto db = drogon::app().getDbClient();
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
    new_conjuction.setRole("owner");
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
  drogon::orm::CoroMapper<drogon_model::notes_db::Notes> noteMapper(db);
  try {
    auto needed_note = co_await noteMapper.findByPrimaryKey(note_id);
    co_return ResponseBuilder::createSuccess("Note details", drogon::HttpStatusCode::k200OK,
                                             needed_note.toJson());
  } catch (const drogon::orm::DrogonDbException& error) {
    LOG_ERROR << "DATABASE ERROR: " << error.base().what();
    co_return ResponseBuilder::createError("Database error",
                                           drogon::HttpStatusCode::k500InternalServerError);
  }
}

auto NoteController::deleteNoteById(const drogon::HttpRequestPtr request, int32_t note_id)
    -> drogon::Task<drogon::HttpResponsePtr> {
  auto db = drogon::app().getDbClient();
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
  auto json_ptr = request->getJsonObject();
  if (!json_ptr || !JsonValidation::myValidate(*json_ptr, {"title"}) ||
      (*json_ptr)["title"].asString().empty())
    co_return ResponseBuilder::createError("Invalid JSON or no JSON provided");
  auto db = drogon::app().getDbClient();
  drogon::orm::CoroMapper<drogon_model::notes_db::Notes> mapper(db);
  try {
    auto user_id = request->attributes()->get<int32_t>("sub");
    drogon::orm::CoroMapper<drogon_model::notes_db::UsersNotes> un_mapper(db);
    auto criteria = drogon::orm::Criteria(drogon_model::notes_db::UsersNotes::Cols::_note_id,
                                          drogon::orm::CompareOperator::EQ, note_id) &&
                    drogon::orm::Criteria(drogon_model::notes_db::UsersNotes::Cols::_user_id,
                                          drogon::orm::CompareOperator::EQ, user_id);
    auto current_conjuction = co_await un_mapper.findBy(criteria);
    if (current_conjuction.empty())
      co_return ResponseBuilder::createError("Note not found",
                                             drogon::HttpStatusCode::k404NotFound);
    auto role = *std::move(current_conjuction).front().getRole();
    if (role != "editor" && role != "owner") {
      co_return ResponseBuilder::createError("Access denied",
                                             drogon::HttpStatusCode::k403Forbidden);
    }
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
