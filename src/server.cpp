#include "server.h"
#include "crow/app.h"
#define CROW_ENFORCE_WS_SPEC
#include "crow/websocket.h"

void HttpWorker::run() {
  crow::SimpleApp server;
  CROW_WEBSOCKET_ROUTE(server, "/ws")
      .onopen([&](crow::websocket::connection &conn) { this->client = &conn; })
      .onclose([&](crow::websocket::connection &conn,
                   const std::string &reason) { this->client = nullptr; });

  server.port(31311).multithreaded().run();
}
void HttpWorker::stream_swap(const std::string &swap_for,
                             const std::string &replacement) {}
