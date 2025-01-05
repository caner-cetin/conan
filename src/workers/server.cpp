#include "server.h"
#include "assets/templates/player_info.h"
#include "assets/util.h"
#include "crow/app.h"
#include "crow/common.h"
#include "crow/http_response.h"
#include <qmutex.h>
#include <qobject.h>
#define CROW_ENFORCE_WS_SPEC
#include "crow/websocket.h"

HttpWorker::HttpWorker(QObject *parent) {
  if (parent) {
    setParent(parent);
  }
}
void HttpWorker::run() {
  CROW_WEBSOCKET_ROUTE(server, "/ws")
      .onopen([&](crow::websocket::connection &conn) {
        QMutexLocker locker(&mutex);
        client = &conn;
      })
      .onclose(
          [&](crow::websocket::connection &conn, const std::string &reason) {
            QMutexLocker locker(&mutex);
            client = nullptr;
          });
  CROW_ROUTE(server, "/")
      .methods(crow::HTTPMethod::GET)([](const crow::request &req) {
        auto response = crow::response();
        response.add_header("Content-Type", "text/html");
        response.write(hex_to_string(Resources::PlayerInfoTemplate::hex));
        return response;
      });
  // do we really need multithreads?
  server.port(31311).run();
}
void HttpWorker::stream_swap(const std::string &swap_for,
                             const std::string &replacement) {}
