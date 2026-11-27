#include "RefreshRepository.h"
#include "RefreshTokens.h"
#include "repositories/RefreshRepository.h"
#include <drogon/orm/Criteria.h>
#include <drogon/orm/Exception.h>
#include <utility>

auto RefreshRepository::GetRefreshToken(std::string &&refresh_token)
    -> drogon::Task<drogon_model::notes_db::RefreshTokens> {
  drogon::orm::CoroMapper<drogon_model::notes_db::RefreshTokens> mapper(db_);
  try {
    co_return co_await mapper.findOne(drogon::orm::Criteria(
        drogon_model::notes_db::RefreshTokens::Cols::_token,
        drogon::orm::CompareOperator::EQ, std::move(refresh_token)));
  } catch (...) {
    throw;
  }
}

auto RefreshRepository::RotateTokens(
    drogon_model::notes_db::RefreshTokens &&old_token,
    drogon_model::notes_db::RefreshTokens &&new_token) -> drogon::Task<void> {
  auto trans = co_await db_->newTransactionCoro();
  try {
    old_token.setIsRevoked(1);
    drogon::orm::CoroMapper<drogon_model::notes_db::RefreshTokens> token_mapper(
        trans);
    co_await token_mapper.update(std::move(old_token));
    co_await token_mapper.insert(std::move(new_token));
  } catch (...) {
    trans->rollback();
    throw;
  }
}

auto RefreshRepository::SignOutFromAllDevices(int32_t user_id)
    -> drogon::Task<void> {
  try {
    co_await db_->execSqlCoro(
        "update refresh_tokens set is_revoked = true where user_id = $1",
        user_id);
  } catch (...) {
    throw;
  }
}
