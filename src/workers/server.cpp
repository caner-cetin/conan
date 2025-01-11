#include "server.h"
#include "assets/templates/player_info.h"
#include "assets/util.h"
#include "conan_util.h"
#include "crow/app.h"
#include "crow/common.h"
#include "crow/http_response.h"
#include "workers/beef.h"
#include <fmt/core.h>
#include <fmt/format.h>
#include <functional>
#include <memory>
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
    auto new_track = columns_to_track(new_player_state->activeItem.columns);
    if (new_track && currently_playing) {
      bool not_same_artist = new_track->artist != currently_playing->artist;
      bool not_same_title = new_track->title != currently_playing->title;
      if (not_same_artist || not_same_title) {
        fetch_and_broadcast_current_and_up_next_track();
      }
    } else if (!currently_playing) {
      fetch_and_broadcast_current_and_up_next_track();
    } else if (!new_track) {
      broadcast_currently_playing(true);
      broadcast_up_next(true);
    }
    current_player_state = std::move(new_player_state);

  } else if (!current_player_state) {
    current_player_state = std::move(new_player_state);
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
  if (queue.first) {
    if ((currently_playing && queue.first->album != currently_playing->album) ||
        (!currently_playing)) {
      auto cover_art = current_player_state->activeItem.artwork();
      if (cover_art) {
        cover_art_changed(*cover_art);
      }
    }
    currently_playing = std::move(queue.first);
    broadcast_currently_playing();
    currently_playing_clear = false;
  } else if (currently_playing_clear == false && !queue.first) {
    broadcast_currently_playing(true);
    currently_playing_clear = true;
    cover_art_changed({});
  }
  if (queue.second) {
    up_next = std::move(queue.second);
    broadcast_up_next();
    up_next_clear = false;
  } else if (up_next_clear == false && !queue.second) {
    broadcast_up_next(true);
    up_next_clear = true;
  }
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

void HttpWorker::broadcast_currently_playing(bool clear) {
  if (current_player_state && client && (currently_playing || clear == true)) {
    json message = {
        {{"UpdateTitle",
          fmt::format("<div id='Title'>{}</div>",
                      clear ? ""
                            : fmt::format("{} - {}", currently_playing->artist,
                                          currently_playing->title))},
         {"UpdateAlbum", fmt::format("<div id='Album'>{}</div>",
                                     clear ? "" : currently_playing->album)},
         {"hx-swap-oob", "true"}}};
    std::string message_str = message.dump();
    client->send_text(message_str);
  }
}

void HttpWorker::broadcast_up_next(bool clear) {
  if (current_player_state && client && (up_next || clear == true)) {
    json message = {
        {{"UpdateUpNextTitle",
          fmt::format(
              "<div id='UpNextTitle'>{}</div>",
              clear ? ""
                    : fmt::format("{} - {}", up_next->artist, up_next->title))},
         {"UpdateUpNextAlbum", fmt::format("<div id='UpNextAlbum'>{}</div>",
                                           clear ? "" : up_next->album)},
         {"hx-swap-oob", "true"}}};
    std::string message_str = message.dump();
    client->send_text(message_str);
  }
}

void HttpWorker::handle_play_pause_button_event() {
  bool paused = current_player_state->playbackState == PlaybackState::Paused;
  bool stopped = current_player_state->playbackState == PlaybackState::Stopped;
  run_function_non_blocking(BeefWeb::Playback::play_pause_toggle);
}

void HttpWorker::handle_skip_button_event() {
  run_function_non_blocking(BeefWeb::Playback::skip);
}

void HttpWorker::handle_stop_button_event() {
  run_function_non_blocking(BeefWeb::Playback::stop);
}

// ⣿⣿⣿⡿⣽⢟⣽⣿⣿⣿⣫⣿⣿⢷⣿⣿⣿⣿⣿⣿⡫⣺⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡞⣿⣿⣷⢿⣿⣿⣿⣿⣿⣿⡏⣿⣻⢿⣝⣽⢼⢿⡇⣟⢼⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⢯⢿⢽⣗⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿
// ⣿⣯⣿⢳⡿⣹⣿⣿⣿⡯⣿⣿⣏⣿⣿⣿⣿⣿⣿⣱⣗⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⢿⣿⣻⣽⣿⣽⣗⣻⣿⣿⢽⣿⣿⢿⣿⢿⣻⣽⡺⡭⡳⡣⣿⣸⣿⡇⡎⣿⣿⣟⣯⣿⣷⣿⢿⣽⣿⣽⣾⣿⣿⣝⣯⢿⣺⣿⡿⣯⣷⣿⡿⣷⣿⢿⣾⣿⣽⣾⣿⣾⣿⣷
// ⣿⣯⣗⣿⢻⣿⡿⣿⣿⣸⣿⡟⣼⣿⣛⣝⠿⣿⣱⣿⢹⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣻⣿⣿⣿⢹⣿⡿⣟⢞⣿⣿⡞⣿⡿⣿⣿⣿⡿⣏⣞⢾⣝⢭⣿⢽⡿⡇⣻⣿⣯⣿⣿⣟⣷⣿⣿⣿⣻⣽⣿⢿⣾⣗⡯⡿⣽⣾⣿⣿⢿⣻⣿⡿⣿⢿⣻⣯⣿⣿⣽⣷⣿⣯
// ⣿⣽⢸⡯⣾⣟⣿⣻⡞⣿⣟⢯⣿⣿⣿⢿⡿⣴⣭⣛⢼⣿⣯⣿⣟⣿⣿⣟⣿⣟⣿⣽⣿⣿⣻⣽⣿⡿⣟⣯⣿⡏⣿⣏⢮⡳⣎⢯⡣⡿⣿⣿⣿⣽⢣⢗⡷⣿⣳⡣⣿⢸⣟⣗⢿⣿⣽⣿⣾⣿⣟⣯⣷⣿⣿⣿⣻⣿⣿⢮⣻⣽⣻⣾⣿⣾⣿⣿⣿⢿⣿⣿⣿⣿⢿⣽⣿⣽⣷⣿
// ⣿⣝⣯⢿⡽⣯⣿⣽⡏⣿⣝⣾⣟⣷⡿⣽⢽⣷⣿⡟⣾⣶⣻⣻⣿⣿⣻⣿⣿⡏⣿⣿⣿⣻⣿⣿⣿⣿⣿⣿⣿⣏⣿⣗⢵⢯⣿⣵⣳⣝⢭⡻⣫⣣⡏⡗⣝⣺⣺⡪⢽⣞⣯⣿⣹⣯⣿⣯⣷⣿⡿⣿⣿⢿⣿⣾⣿⢿⣻⡹⣗⣟⡎⣿⣾⣿⣽⣿⣾⣿⣿⢷⣿⡿⣿⡿⣯⣿⣿⣽
// ⣿⢇⡿⣸⣯⢷⣟⣾⢸⣟⢧⡷⣟⡷⣣⣣⣿⣿⣻⣏⣿⢇⣿⢷⣿⣻⣿⣻⣽⡧⣿⣿⣿⣿⢿⣻⣽⣾⣿⣿⣻⣯⣿⣗⢵⣫⢾⣫⢞⣎⢧⢝⢿⡿⣪⡫⣞⢾⢾⢝⡼⡳⣽⣾⢺⣿⣻⣟⣿⣽⣿⣟⣿⣿⣻⣾⣿⣿⡟⣼⡯⣯⣳⣿⣿⣽⣿⣽⣿⣽⣾⣿⣿⢿⡿⣿⡿⣯⣿⣯
// ⣿⢸⣏⡿⣞⣯⡷⣿⣸⢿⢸⣫⣗⣯⢷⣿⣟⣿⡿⣿⢸⡇⣿⣟⣿⣽⣾⡿⣟⡧⣿⡿⣷⣿⣿⣿⣿⢿⠿⣟⢿⣷⣻⣷⢣⣳⢽⣾⣻⡪⣏⣧⣷⡯⣷⣧⣳⣹⢹⢕⡽⣝⣽⡾⣇⣿⣿⣿⢿⣿⣽⣿⣻⣽⣿⣯⣷⣿⣗⡯⡿⡽⣺⣿⣽⣿⣾⡿⣯⣿⣿⣻⣾⣿⣿⣿⢿⣿⣟⣿
// ⢿⣸⢳⣻⡯⣷⣟⣷⣺⢿⣸⢐⢬⡦⣕⠬⣙⠻⣟⣿⣏⣯⢺⣽⡯⣿⢾⣟⣿⣗⣿⢿⣻⣷⢿⡮⣾⢿⣻⣽⣾⡎⣮⣟⡜⡮⣻⢺⢪⣺⣾⣿⣿⢹⣿⣷⢵⡟⣞⣯⢓⣿⣸⣟⣷⢽⣿⣾⣿⣿⣽⡿⣿⡿⣟⣯⣿⣿⢱⣟⣯⣳⣿⣿⢿⣽⣾⣿⣿⢿⣯⣿⡿⣟⣿⣽⣿⣟⣿⣿
// ⣿⢽⣸⣯⣟⣷⣻⣞⢾⢯⢾⢌⣿⡿⡱⣱⡲⢭⡊⡗⡗⣾⣝⠖⣿⢽⣻⡽⣷⡳⣟⣟⣯⡿⣯⢹⣻⢼⢿⣻⣽⢜⣽⣯⣷⢽⡬⣷⠿⣽⣿⣽⣾⢹⣿⡟⡾⡵⣿⢝⣽⢸⡞⣾⣺⠽⣿⣽⣿⣾⡿⣿⡿⣿⣿⣿⢿⡮⣟⡾⣇⣿⣿⣾⣿⣿⢿⣽⣾⣿⣿⣻⣿⣿⣿⣟⣯⣿⣿⣾
// ⣽⡽⣸⣾⣳⣟⣾⣳⣻⡽⡽⣧⢺⣇⣻⣽⢌⢆⢵⡅⢝⣷⣿⣿⣵⣝⣯⡻⢽⡸⣯⢿⣺⣯⢿⢨⡟⢼⣻⣽⣻⢔⣿⣺⣽⢿⢸⣿⡏⣿⢾⣯⡯⣟⣯⢗⣷⣻⢏⣾⢯⡮⣞⢼⣺⡣⣿⣿⣯⣷⣿⣿⣿⡿⣿⣾⡿⣹⣽⢽⣺⣿⣷⣿⣿⣽⣿⣿⣿⢿⣻⣿⣯⣷⣿⡿⣿⣻⣯⣿
// ⣿⢘⣿⣺⣗⣿⣺⣗⢗⡯⣿⣿⣯⣷⡜⡾⣷⢷⡟⣊⡧⣿⣿⣽⣾⣿⣾⣟⣯⣿⣺⣭⣗⢯⣻⢼⣹⡽⡽⣾⠽⣝⣞⣷⣻⡝⡽⣾⢺⣟⣯⣷⣏⣿⢯⢻⡾⣝⣾⢯⣾⣯⢺⣍⢾⣝⢽⣿⣽⣿⢿⣽⣾⣿⣿⣯⣫⣟⡞⣽⣟⣷⣿⣷⣿⣯⣿⣾⣿⢿⣿⣯⣿⣟⣿⡿⣿⡿⣿⣻
// ⣿⡎⣷⣟⣾⣳⣟⣾⠸⣏⣿⣾⣿⣾⣟⣾⣴⣵⣼⣿⡿⣿⣟⣿⣟⣿⣽⣿⢿⣽⣿⣷⡟⢟⢚⢝⢚⡚⡽⡺⣽⣇⣛⢮⢭⣣⡹⡯⣿⣺⢷⢷⢳⢯⢏⣾⢟⣾⢳⢟⡻⣿⣏⢾⡱⣗⣝⣿⣽⣿⡿⣿⡿⣿⣾⣣⡷⡯⣽⣿⡿⣿⣽⣾⣿⣽⣿⣽⣾⣿⣿⣽⣿⣽⣿⡿⣟⣿⣿⣿
// ⣾⡯⣷⣟⣾⣳⣟⡾⡵⣫⢻⣯⣿⣾⣿⢿⣻⣿⢿⣻⣿⣿⢿⣟⣿⣿⣟⣿⣿⡿⣿⣮⣾⠏⣔⣗⢝⢖⡦⡅⡕⢕⣻⢇⡿⣺⣽⣞⡮⣛⣽⡟⡾⣝⣓⡷⣛⡼⣮⢯⢯⢺⣿⣪⢧⣻⣪⢾⣿⣯⣿⣿⡿⣿⡫⣾⡝⣽⣿⣟⣿⣿⢿⣿⣻⣽⣿⣻⣿⣟⣯⣿⣯⣿⣟⣿⣿⡿⣿⣽
// ⢿⡯⣷⣻⣞⡷⣯⡇⣿⢐⢯⣿⡿⣟⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣯⣿⣿⣽⣿⣿⢿⣿⡑⣷⣳⢔⢥⣿⡪⣺⡧⡪⢪⡯⣿⣻⢾⢝⣗⢧⣧⣫⡣⣪⢾⣳⡿⣿⣻⣽⢸⣿⣷⣹⡜⣮⣳⢽⣿⣽⣾⣿⢏⣾⢗⣾⣿⣿⣻⣯⣿⣿⣟⣿⣟⣿⣻⣽⣿⣟⣿⣽⣿⣻⣯⣷⣿⣿⣿
// ⣿⣯⢳⣟⣾⣻⡽⡇⣿⠸⣮⢺⣿⣿⣿⣯⣷⣿⣯⣷⣿⣿⣿⣟⣷⣿⢿⣽⣿⣻⣾⣿⣻⢷⣕⢟⠿⠽⣚⣼⡿⡹⣸⢸⢽⡾⣽⣟⡷⡵⡝⠿⠯⡯⣮⢯⣻⢽⢯⡷⣓⣿⣿⡿⣷⡕⡗⣗⣗⣻⣿⣻⣟⡽⣮⡿⣿⣿⣽⣿⣟⣯⣷⣿⣿⢿⣻⣿⡿⣟⣿⣟⣿⣟⣿⡿⣟⣿⣿⣽
// ⠽⣿⢸⣟⣾⣳⡿⡇⣿⢨⣺⣷⣽⣿⣽⣿⣟⣿⣟⣿⣿⡿⣿⢽⡯⣟⡿⣯⣟⣯⡿⣽⣽⣻⢮⣗⡿⣟⣯⣟⣾⣻⡽⣝⢷⣿⣻⣳⢽⡹⣺⣻⣝⣮⡯⣻⣽⣿⣽⣺⢹⣿⣻⣿⢿⣻⣎⢗⢗⣧⡻⣯⡺⣵⣿⢿⣟⣯⣿⣯⣿⣿⢿⣟⣿⣿⡿⣟⣿⣿⣿⣻⣿⣻⣿⢿⣿⣿⣻⣽
// ⣟⡷⣝⢷⡯⣷⣟⡧⣻⢼⡪⣿⣿⣻⣿⣽⣿⣟⣿⡿⣯⣷⣻⢯⣟⣯⣟⡷⣯⢷⣟⣷⣻⣞⡿⣽⣽⣻⣞⣷⣻⢾⡝⡾⢽⢺⣪⣞⣮⢳⢽⣺⢸⣿⡿⣮⣞⢾⣳⢭⣿⣿⢿⣻⣿⣿⣿⣯⡧⡳⣳⣕⢿⣿⢿⣿⢿⣿⣿⣻⣽⣾⣿⣿⡿⣷⣿⣿⣿⣿⣾⣿⣟⣿⡿⣿⢿⣾⣿⣿
// ⣾⣟⣗⢿⡽⡷⣯⡇⡯⣞⡯⡸⣿⣻⣿⣯⣿⣿⡻⠿⡿⣷⣻⣽⣻⣞⡷⣟⣯⣟⣷⣻⣞⡷⣟⣟⣾⣳⣟⣾⢽⠫⣪⢯⣯⢿⣺⣞⢎⣯⢺⣕⣿⣿⣿⢿⣿⣷⡷⣿⣟⣿⣿⣿⢿⣷⢟⣫⡾⣫⣮⡺⣕⣟⢿⡿⣿⣿⣽⣿⡿⣿⣿⣻⣿⢿⣻⣯⣿⣾⣿⣾⣿⢿⣿⡿⣿⣿⣻⣾
// ⢿⣯⡗⣿⢽⡿⣽⡇⣯⢻⣡⣶⡚⢿⣻⣽⣿⣾⡌⣏⢮⡪⡚⣷⣻⣞⣯⣟⣷⣻⣞⡷⣯⢿⣽⡽⣞⣷⡻⣎⢷⡕⣯⢿⡽⡯⡷⡽⣺⡳⡱⡧⣿⣿⣾⣿⣿⣾⣿⢿⣻⣯⣷⢟⣯⣳⣻⢝⣾⣿⡿⣷⣇⡧⡳⡽⢿⣯⣿⣯⣿⣿⣯⣿⡿⣿⡿⣿⣻⣯⣿⣾⣿⢿⣻⣿⣟⣯⣿⣿
// ⣿⢿⣻⢼⢯⣟⡷⣧⢯⣺⢾⣽⣻⡎⢿⣿⣿⣻⣿⡜⡎⣎⣗⣷⣻⣞⣷⣻⣞⡷⣯⢿⣽⣻⢾⡽⡯⣳⢵⢯⡗⣽⡽⣯⢿⢽⢽⣱⡗⣽⢽⣸⣿⣷⣿⣷⣿⢿⣾⡿⣟⢯⢾⡽⡾⡽⣼⣿⣿⣿⣻⣿⡿⣿⣯⡺⡳⣝⢿⣿⣽⣷⣿⡿⣿⡿⣿⣿⡿⣿⣻⣽⣾⣿⣿⣿⣻⣿⢿⣻
// ⣿⣿⣿⣞⡽⡾⣽⣳⡳⣽⣻⣞⣷⣻⢔⠹⢿⣻⣿⢽⡯⣿⢽⣞⣷⣻⣞⡷⣯⢿⢽⡻⢞⢝⢭⣺⢾⡽⣽⣳⢽⢯⡿⡽⡽⡽⣱⣺⣱⢇⡯⣿⣟⣯⣷⣿⢿⡟⣯⢞⢵⣽⣳⢿⢝⣽⣿⣟⣿⣾⣿⣻⣿⢿⡿⣿⣾⢪⢷⡹⣿⣽⣷⣿⣿⡿⣿⣷⣿⣿⣿⡿⣿⣿⣻⣽⣿⣻⣿⣿
// ⣷⣿⣷⡗⣯⢟⣗⡯⡎⣷⣻⣞⣷⣻⢧⠣⡑⢝⠯⠿⠽⡫⢟⢓⢛⠪⡓⢍⢢⢣⡱⣨⣶⣟⣯⣿⡽⣯⡷⣳⣟⡯⡯⡯⡯⣓⣞⢖⡷⢵⢳⣿⣿⢿⣿⣻⣫⣞⡷⡯⣟⣾⣺⣫⣿⣿⣯⣿⣿⣽⣾⣿⢿⣿⢿⣟⣿⣷⣣⢻⣚⢿⣯⣿⣷⣿⣿⣷⣿⣿⣾⣿⢿⣻⣿⣟⣿⣟⣯⣷
// ⣿⣿⣽⣷⢹⣽⣺⢽⢕⣯⡷⣟⣾⡽⡿⣕⡌⡢⡑⢅⠕⡌⣢⡱⣌⡮⣮⣞⡾⣪⣾⡿⣷⣟⣷⢯⣿⣝⣮⢷⢯⢯⢯⠯⣝⡼⣪⣗⢧⢟⣵⡻⣿⡿⣟⣽⣺⡾⣵⣟⣟⣞⢧⣿⣿⣾⣿⣯⣿⣟⣯⣿⣿⣿⡿⣿⣟⣿⣷⣹⢼⢝⣿⣿⣽⣿⣾⣿⣷⣿⣷⣿⣿⣿⣿⣻⣿⣻⣿⣿
// ⣾⣿⣽⡿⡸⣺⣺⢽⢸⣞⣯⣟⣷⣻⢿⡽⣗⣷⢯⣷⣻⡽⣗⣿⢽⡯⣷⢯⣾⣿⣟⣿⣻⡾⣯⢿⡳⡾⡽⡽⡽⡽⣝⢝⡮⣽⡳⢧⣛⣞⡮⣯⢽⡿⣳⢯⣾⢽⣿⣾⣯⡯⣾⣿⣷⣿⣾⣿⣽⡿⣟⣿⣽⣾⣿⡿⣟⣿⣻⣧⢳⢝⡞⣿⣽⣷⣿⣾⣿⣾⣿⣾⣿⣾⣿⣻⣽⣿⣯⣿
// ⣿⣽⡿⣳⣿⢱⢯⣻⡸⣮⡳⣿⣺⣽⢯⡿⣽⢾⣻⡾⣽⢯⡿⣽⢯⣟⢧⣿⣿⣯⣿⣟⣯⣿⢟⣵⣟⡯⡯⡯⡯⣯⢺⣝⣾⢽⠵⣫⣾⡳⡯⣗⡷⡕⣿⣹⡏⣿⣳⣟⣷⢻⣿⡿⣾⣟⣷⣿⣟⣿⣿⣿⢿⣿⣽⣿⣿⣿⡿⣿⡽⣕⢽⢻⣿⣯⣿⣷⣿⣷⣿⣾⣿⣾⡿⣿⣟⣯⣿⣟
// ⣿⡿⢇⣿⣿⣝⣽⡺⣇⣿⣯⢳⡿⣞⣿⢽⣻⡯⣯⡿⣽⢯⡿⣽⢯⣵⣿⣿⣿⣽⣿⣻⣿⣫⢾⣳⢗⡯⡯⣯⢳⢵⢏⢞⣎⣯⣾⣻⢾⡽⡯⣗⡯⣷⢹⢼⣺⣟⡾⣽⢮⣿⡿⣿⡿⣿⢿⣟⣿⡿⣷⣿⣿⡿⣯⣿⣾⡿⣿⡿⣇⢷⡹⣇⣿⣯⣿⣾⣿⣷⡿⣿⣾⡿⣿⣿⣻⣿⢿⣿
// ⣾⣿⢲⡱⣟⣿⣜⣞⣗⢿⣿⣿⢽⣻⣾⣻⡽⣯⢯⣟⡽⡯⡻⢽⣺⣿⣿⣿⣯⣿⣯⣿⢵⣽⣻⣺⢽⢽⢝⡞⣞⡽⣮⢷⣻⢾⣳⣟⡯⡿⡽⣳⢟⢾⢝⣺⢵⡯⣿⢽⣱⣿⣿⣟⣿⣿⢿⣿⣟⣿⣿⣻⣾⣿⡿⣿⣽⣿⣿⣻⣿⣱⢹⣳⢹⣿⣯⣿⣾⣷⣿⣿⣷⣿⣿⣟⣿⣻⣿⡿
// ⣿⡯⣾⣿⣮⣫⡿⣮⢞⢽⣿⣻⣿⣿⣾⢿⣿⣿⣿⢿⣿⣿⡟⣽⣿⣿⣿⣯⣿⣽⡿⣪⣟⣞⣞⡮⡯⣳⣽⢾⣯⡿⣯⢿⣽⣽⢾⡳⡟⢷⠻⡗⣯⢮⣫⢺⢽⡽⣽⡳⣿⡿⣷⣿⣿⣻⣿⣟⣿⣟⣯⣿⣿⣯⣿⣿⢿⣽⣾⣿⣿⢜⡮⡯⣺⣿⣯⣿⣿⣽⣿⢷⣿⡿⣷⣿⣿⡿⣿⡿