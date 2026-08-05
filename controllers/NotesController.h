#pragma once

#include <drogon/HttpController.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <drogon/HttpTypes.h>
#include <drogon/drogon.h>

class NoteController : public drogon::HttpController<NoteController> {

public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(NoteController::createNewNote, "/api/v1/notes", drogon::Post, "AuthFilter",
                "JsonParserFilter");
  ADD_METHOD_TO(NoteController::getNoteById, "/api/v1/notes/{id}", drogon::Get, "AuthFilter",
                "AccessFilter");
  ADD_METHOD_TO(NoteController::deleteNoteById, "/api/v1/notes/{id}", drogon::Delete, "AuthFilter",
                "AccessFilter");
  ADD_METHOD_TO(NoteController::editNoteById, "/api/v1/notes/{id}", drogon::Patch, "AuthFilter",
                "AccessFilter", "JsonParserFilter");
  ADD_METHOD_TO(NoteController::getAllHeadsOfNotes, "/api/v1/notes", drogon::Get, "AuthFilter");
  ADD_METHOD_TO(NoteController::addRoleByUserId, "/api/v1/notes/roles/{id}", drogon::Post,
                "AuthFilter", "AccessFilter", "JsonParserFilter");
  METHOD_LIST_END

  auto createNewNote(const drogon::HttpRequestPtr request) -> drogon::Task<drogon::HttpResponsePtr>;
  auto editNoteById(const drogon::HttpRequestPtr request, int32_t note_id)
      -> drogon::Task<drogon::HttpResponsePtr>;
  auto deleteNoteById(const drogon::HttpRequestPtr request, int32_t note_id)
      -> drogon::Task<drogon::HttpResponsePtr>;
  auto getNoteById(const drogon::HttpRequestPtr request, int32_t note_id)
      -> drogon::Task<drogon::HttpResponsePtr>;
  auto getAllHeadsOfNotes(const drogon::HttpRequestPtr request)
      -> drogon::Task<drogon::HttpResponsePtr>;

  auto addRoleByUserId(const drogon::HttpRequestPtr request, int32_t note_id)
      -> drogon::Task<drogon::HttpResponsePtr>;
};