#pragma once

#include <string>

struct Url {
  std::string scheme;
  std::string user;
  std::string password;
  std::string host;
  std::string port;
  std::string path;
  std::string query;
  std::string fragment;

  Url() = default;
  Url(const Url&) = default;
  ~Url() = default;
  Url& operator=(const Url&) = default;
};
