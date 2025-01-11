#include "server.h"
#include "assets/templates/player_info.h"
#include "assets/util.h"
#include "conan_util.h"
#include "crow/app.h"
#include "crow/common.h"
#include "crow/http_response.h"
#include "workers/beef.h"
#include <fmt/format.h>
#include <mutex>
#include <qmutex.h>
#include <qnamespace.h>
#include <qobject.h>
#include <qtimer.h>
#include <spdlog/spdlog.h>
#include <utility>
#define CROW_ENFORCE_WS_SPEC
#include "crow/websocket.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

HttpWorker::HttpWorker(QObject *parent) : QThread(parent) {}

void HttpWorker::run() {
  CROW_WEBSOCKET_ROUTE(server, "/ws")
      .onopen([&](crow::websocket::connection &conn) {
        spdlog::debug("initiating new websocket connection");
        std::lock_guard<std::mutex> _(mutex);
        client = &conn;
        broadcast_all_properties();
      })
      .onclose(
          [&](crow::websocket::connection &conn, const std::string &reason) {
            spdlog::debug("websocket connection closed");
            std::lock_guard<std::mutex> _(mutex);
            client = nullptr;
          });

  CROW_ROUTE(server, "/")
      .methods(crow::HTTPMethod::GET)(
          [](const crow::request &req, crow::response &res) {
            res.add_header("Content-Type", "text/html");
            res.write(hex_to_string(Resources::PlayerInfoTemplate::hex));
            auto garbage_split = split(res.body, "</html>");
            res.body = garbage_split[0] + "</html>";
            res.end();
          });
  // https://crowcpp.org/master/guides/app/#using-the-app
  // When using run_async(), make sure to use a variable to save the function's
  // output (such as auto _a = app.run_async()). Otherwise the app will run
  // synchronously.
  auto _a = server.port(31311).run_async();

  player_poll = new QTimer();
  player_poll->moveToThread(this);

  connect(player_poll, &QTimer::timeout, this, &HttpWorker::update_player_state,
          Qt::DirectConnection);
  player_poll->start(250);
  spdlog::debug("starting http worker event loop");
  exec();
}

void HttpWorker::update_player_state() {
  auto new_player_state = BeefWeb::PlayerState::query();
  if (!new_player_state) {
    spdlog::warn("new player state is null");
    return;
  }

  if (current_player_state) {
    if (new_player_state->playbackState !=
        current_player_state->playbackState) {
      broadcast_player_state();
    }

    current_player_state = std::move(new_player_state);

  } else if (!current_player_state) {
    current_player_state = std::move(new_player_state);
  }
  auto new_track = columns_to_track(new_player_state->activeItem.columns);
  if (currently_playing) {
    if (new_track->artist != currently_playing->artist &&
        new_track->title != currently_playing->title) {
      fetch_and_broadcast_current_and_up_next_track(); 
    }
    currently_playing = std::move(new_track);
  } else if (!currently_playing) {
    currently_playing = std::move(new_track);
  }
}

void HttpWorker::broadcast_all_properties() {
  current_player_state = BeefWeb::PlayerState::query();
  broadcast_player_state();
  fetch_and_broadcast_current_and_up_next_track();
}

void HttpWorker::fetch_and_broadcast_current_and_up_next_track() {
  auto queue = BeefWeb::PlaylistItems::current_and_next_track(
      &current_player_state->activeItem);
  currently_playing = std::move(queue.first);
  up_next = std::move(queue.second);
  broadcast_currently_playing();
  broadcast_up_next();
}
void HttpWorker::broadcast_player_state() {
  if (current_player_state && client) {
    json message = {{{"UpdatePlaybackState",
                      fmt::format("<div id='PlaybackState'>{}</div>",
                                  current_player_state->playbackState)},
                     {"hx-swap-oob", "true"}}};
    std::string message_str = message.dump();
    client->send_text(message_str);
  } else {
    spdlog::warn("player state changed but current player state or client is "
                 "null, cannot stream the event");
  }
}

void HttpWorker::broadcast_currently_playing() {
  if (current_player_state && client && currently_playing) {
    json message = {{{"UpdateTitle", fmt::format("<div id='Title'>{}</div>",
                                                 currently_playing->title)},
                     {"UpdateAlbum", fmt::format("<div id='Album'>{}</div>",
                                                 currently_playing->album)},
                     {"hx-swap-oob", "true"}}};
    std::string message_str = message.dump();
    client->send_text(message_str);
  } else {
    spdlog::warn("cannot stream currenlt playing due to current player state "
                 "or client being null");
  }
}

void HttpWorker::broadcast_up_next() {
  if (current_player_state && client && up_next) {
    json message = {
        {{"UpdateUpNextTitle",
          fmt::format("<div id='UpNextTitle'>{}</div>", up_next->title)},
         {"UpdateUpNextAlbum",
          fmt::format("<div id='UpNextAlbum'>{}</div>", up_next->album)},
         {"hx-swap-oob", "true"}}};
    std::string message_str = message.dump();
    client->send_text(message_str);
  }
}