#pragma once
#include "RefreshTokens.h"
#include "repositories/BasicRepository.h"
#include <drogon/drogon.h>

class RefreshRepository
    : public BasicRepository<drogon_model::notes_db::RefreshTokens> {
public:
  using BasicRepository<drogon_model::notes_db::RefreshTokens>::BasicRepository;
  auto GetRefreshToken(std::string &&refresh_token)
      -> drogon::Task<drogon_model::notes_db::RefreshTokens>;
  auto SignOutFromAllDevices(int32_t user_id) -> drogon::Task<void>;
  auto RotateTokens(drogon_model::notes_db::RefreshTokens &&old_token,
                    drogon_model::notes_db::RefreshTokens &&new_token)
      -> drogon::Task<void>;
};
