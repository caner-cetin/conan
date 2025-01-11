

#include "workers/beef.h"
#include "conan_util.h"
#include <algorithm>
#include <cstdlib>
#include <curl/curl.h>
#include <curl/easy.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <memory>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <variant>
using namespace nlohmann;
using namespace BeefWeb;
size_t CurlWrite_CallbackFunc_StdString(void *contents, size_t size,
                                        size_t nmemb, std::string *s);
const char *get_api_url() {
  const char *env_url = getenv("FOOBAR_API_URL");
  return env_url ? env_url : "http://192.168.56.1:8880/api";
}

std::unique_ptr<PlayerState::Player> PlayerState::query() {
  CURL *client = curl_easy_init();
  if (!client) {
    spdlog::warn("cannot initialize curl client");
    return nullptr;
  }
  std::string body;
  curl_easy_setopt(client, CURLOPT_URL,
                   fmt::format("{}/player?columns={}", get_api_url(),
                               fmt::join(TrackQueryColumns, ","))
                       .c_str());
  curl_easy_setopt(client, CURLOPT_WRITEFUNCTION,
                   CurlWrite_CallbackFunc_StdString);
  curl_easy_setopt(client, CURLOPT_WRITEDATA, &body);
  CURLcode op = curl_easy_perform(client);
  if (op != CURLE_OK) {
    spdlog::warn("cannot query player state: {}", curl_easy_strerror(op));
    curl_easy_cleanup(client);
    return nullptr;
  }
  curl_easy_cleanup(client);
  json data = json::parse(body);
  return std::make_unique<PlayerState::Player>(
      data.get<PlayerState::State>().player);
}

std::pair<std::unique_ptr<Track>, std::unique_ptr<Track>>
PlaylistItems::current_and_next_track(PlayerState::ActiveItem *active_item) {
  CURL *client = curl_easy_init();
  if (!client) {
    spdlog::warn("cannot initialize curl client");
    return {nullptr, nullptr};
  }
  std::string body;
  curl_easy_setopt(client, CURLOPT_URL,
                   fmt::format("{}/playlists/{}/items/{}:2?columns={}",
                               get_api_url(), active_item->playlistId,
                               active_item->index,
                               fmt::join(TrackQueryColumns, ","))
                       .c_str());
  curl_easy_setopt(client, CURLOPT_WRITEFUNCTION,
                   CurlWrite_CallbackFunc_StdString);
  curl_easy_setopt(client, CURLOPT_WRITEDATA, &body);
  CURLcode op = curl_easy_perform(client);
  if (op != CURLE_OK) {
    spdlog::warn("cannot query player state: {}", curl_easy_strerror(op));
    curl_easy_cleanup(client);
    return {nullptr, nullptr};
  }
  curl_easy_cleanup(client);
  json data = json::parse(body);
  Playlist playlist = data.get<PlaylistItems::Playlist>();
  if (playlist.playlistItems.items.empty()) {
    spdlog::info("there are no tracks in queue");
    curl_easy_cleanup(client);
    return {nullptr, nullptr};
  }
  for (auto &item : playlist.playlistItems.items) {
    if (item.columns.size() != 7) {
      spdlog::error("Invalid columns size: {}", item.columns.size());
      continue;
    }

    for (auto &column : item.columns) {
      ReplaceAll(trim(column), "\"", "");
    }
  }

  auto queue = playlist.playlistItems.items;
  return {columns_to_track(queue[0].columns),
          columns_to_track(queue[1].columns)};
}

// https://stackoverflow.com/a/36401787/22757599
size_t CurlWrite_CallbackFunc_StdString(void *contents, size_t size,
                                        size_t nmemb, std::string *s) {
  size_t newLength = size * nmemb;
  try {
    s->append((char *)contents, newLength);
  } catch (std::bad_alloc &e) {
    // handle memory problem
    return 0;
  }
  return newLength;
}

std::unique_ptr<Track> columns_to_track(std::vector<std::string> columns) {
  try {
    if (columns.size() != 7) {
      spdlog::error("Invalid columns size in to_track: {}", columns.size());
      return nullptr;
    }

    // Make a safe copy
    std::vector<std::string> c = columns;

    if (c[4].find("/") != std::string::npos) {
      c[4] = split(c[4], "/")[0];
    }

    if (c[4].empty() || !std::all_of(c[4].begin(), c[4].end(), [](char ch) {
          return std::isdigit(ch) || ch == '-';
        })) {
      spdlog::error("Invalid track number format: {}", c[4]);
      return nullptr;
    }

    return std::make_unique<Track>(
        Track{c[0], c[1], c[2], c[3], static_cast<int8_t>(std::stoi(c[4])),
              c[5] != "?" ? static_cast<int8_t>(std::stoi(c[5]))
                          : static_cast<int8_t>(0),
              c[6]});
  } catch (const std::exception &e) {
    spdlog::error("Exception in to_track: {}", e.what());
    return nullptr;
  }
}