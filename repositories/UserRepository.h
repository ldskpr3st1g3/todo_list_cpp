#pragma once
#include "Users.h"
#include "repositories/BasicRepository.h"
#include <drogon/drogon.h>
#include <drogon/orm/CoroMapper.h>
#include <drogon/orm/Criteria.h>

class UserRepository : public BasicRepository<drogon_model::notes_db::Users> {

public:
  using BasicRepository<drogon_model::notes_db::Users>::BasicRepository;

  auto getUserByLogin(const std::string& login)
      -> drogon::Task<std::optional<drogon_model::notes_db::Users>>;
  auto getUserByEmail(const std::string& email)
      -> drogon::Task<std::optional<drogon_model::notes_db::Users>>;
};