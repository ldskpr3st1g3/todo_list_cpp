#pragma once
#include <drogon/drogon.h>
#include <exception>
#include <format>
#include <string_view>

namespace ErrorLogger {

inline auto printDatabaseError(std::string_view func, const drogon::orm::DrogonDbException& e)
    -> void {
  LOG_ERROR << std::format("Database error in {}: ", func) << e.base().what();
}

inline auto printThreadPoolError(std::string_view func, const std::exception& e) -> void {
  LOG_ERROR << std::format("Threadpool error in {}: ", func) << e.what();
}

inline auto printJwtError(std::string_view func, const std::exception& e) -> void {
  LOG_ERROR << std::format("Jwt error in {}: ", func) << e.what();
}

} // namespace ErrorLogger