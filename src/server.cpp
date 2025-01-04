#include "server.h"
#include "assets/templates/player_info.h"
#include "assets/util.h"
#include "crow/app.h"
#include "crow/common.h"
#include "crow/http_response.h"
#define CROW_ENFORCE_WS_SPEC
#include "crow/websocket.h"

void HttpWorker::run() {
  crow::SimpleApp server;
  CROW_WEBSOCKET_ROUTE(server, "/ws")
      .onopen([&](crow::websocket::connection &conn) { this->client = &conn; })
      .onclose([&](crow::websocket::connection &conn,
                   const std::string &reason) { this->client = nullptr; });
  CROW_ROUTE(server, "/")
      .methods(crow::HTTPMethod::GET)([](const crow::request &req) {
        auto response = crow::response();
        response.add_header("Content-Type", "text/html");
        response.write(hex_to_string(Resources::PlayerInfoTemplate::hex));
        return response;
      });
  server.port(31311).multithreaded().run();
}
void HttpWorker::stream_swap(const std::string &swap_for,
                             const std::string &replacement) {}
