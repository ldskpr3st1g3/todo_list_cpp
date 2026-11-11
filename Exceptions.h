#pragma once
#include <stdexcept>
#include <string>

class ThreadPoolException : public std::runtime_error {
public:
  explicit ThreadPoolException(const std::string& e) : std::runtime_error(e) {}
};

class JwtException : public std::runtime_error {
public:
  explicit JwtException(const std::string& e) : std::runtime_error(e) {}
};