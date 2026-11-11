#pragma once
#include <drogon/drogon.h>
#include <drogon/orm/CoroMapper.h>
#include <drogon/orm/Exception.h>
#include <vector>

template <typename T> class BasicRepository {
protected:
  drogon::orm::DbClientPtr db_;

public:
  explicit BasicRepository(drogon::orm::DbClientPtr db) : db_(std::move(db)) {}

  auto getAll() -> drogon::Task<std::vector<T>> {
    try {

      drogon::orm::CoroMapper<T> T_mapper(db_);
      co_return co_await T_mapper.findAll();
    } catch (...) {
      throw;
    }
  }

  auto getById(int pk) -> drogon::Task<T> {
    try {
      drogon::orm::CoroMapper<T> T_mapper(db_);
      co_return co_await T_mapper.findByPrimaryKey(pk);
    } catch (...) {
      throw;
    }
  }

  auto insert(T object) -> drogon::Task<T> {
    try {

      drogon::orm::CoroMapper<T> T_mapper(db_);
      co_return co_await T_mapper.insert(std::move(object));
    } catch (...) {
      throw;
    }
  }

  auto deleteById(int pk) -> drogon::Task<int> {
    try {

      drogon::orm::CoroMapper<T> T_mapper(db_);
      co_return co_await T_mapper.deleteByPrimaryKey(pk);
    } catch (...) {
      throw;
    }
  }
};