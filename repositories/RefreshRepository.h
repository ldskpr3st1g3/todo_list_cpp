#pragma once
#include "RefreshTokens.h"
#include "repositories/BasicRepository.h"
#include <drogon/drogon.h>

class RefreshRepository : public BasicRepository<drogon_model::notes_db::RefreshTokens> {
public:
  using BasicRepository<drogon_model::notes_db::RefreshTokens>::BasicRepository;
};