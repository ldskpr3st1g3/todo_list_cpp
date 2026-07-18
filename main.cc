#include <drogon/drogon.h>
#include <sodium.h>

int main() {
  if (sodium_init() < 0) {
    LOG_ERROR << "FATAL: failed to initialize Libsodium";
    return 1;
  }
  LOG_INFO << "Libsodium initialized successfully.";
  drogon::app().loadConfigFile("../config.json");

  drogon::app().run();
  LOG_INFO << "server is running";
  return 0;
}
