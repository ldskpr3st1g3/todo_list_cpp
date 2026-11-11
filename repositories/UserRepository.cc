#include "repositories/UserRepository.h"
#include "Users.h"
#include <drogon/orm/CoroMapper.h>

auto UserRepository::getUserByLogin(const std::string& login)
    -> drogon::Task<std::optional<drogon_model::notes_db::Users>> {
  try {

    drogon::orm::CoroMapper<drogon_model::notes_db::Users> user_mapper(this->db_);
    auto needed_user = co_await user_mapper.findBy(drogon::orm::Criteria(
        drogon_model::notes_db::Users::Cols::_login, drogon::orm::CompareOperator::EQ, login));
    if (needed_user.empty())
      co_return std::nullopt;
    co_return needed_user.front();
  } catch (...) {
    throw;
  }
}

auto UserRepository::getUserByEmail(const std::string& email)
    -> drogon::Task<std::optional<drogon_model::notes_db::Users>> {
  try {
    drogon::orm::CoroMapper<drogon_model::notes_db::Users> user_mapper(this->db_);
    auto needed_user = co_await user_mapper.findBy(drogon::orm::Criteria(
        drogon_model::notes_db::Users::Cols::_email, drogon::orm::CompareOperator::EQ, email));
    if (needed_user.empty())
      co_return std::nullopt;
    co_return needed_user.front();
  } catch (...) {
    throw;
  }
}
