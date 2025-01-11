

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
size_t CurlWrite_CallbackFunc_StdBytes(void *contents, size_t size,
                                       size_t nmemb, void *userp);
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
  if (active_item->playlistId == "") {
    return {nullptr, nullptr};
  }
  CURL *client = curl_easy_init();
  if (!client) {
    spdlog::warn("cannot initialize curl client");
    return {nullptr, nullptr};
  }
  std::string body;
  std::string target = fmt::format(
      "{}/playlists/{}/items/{}:2?columns={}", get_api_url(),
      active_item->playlistId,                           // playlist id
      active_item->index == -1 ? 0 : active_item->index, // offset
      fmt::join(TrackQueryColumns, ",")); // which columns to retrieve
  curl_easy_setopt(client, CURLOPT_URL, target.c_str());
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
  if (queue.size() == 1) {
    return {columns_to_track(queue[0].columns), nullptr};
  }
  return {columns_to_track(queue[0].columns),
          columns_to_track(queue[1].columns)};
}

std::unique_ptr<std::vector<unsigned char>> PlayerState::ActiveItem::artwork() {
  CURL *curl = curl_easy_init();

  std::vector<unsigned char> image;
  curl_easy_setopt(curl, CURLOPT_URL,
                   fmt::format("{}/artwork/{}/{}", get_api_url(),
                               this->playlistId, this->index)
                       .c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
                   CurlWrite_CallbackFunc_StdBytes);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &image);

  CURLcode op = curl_easy_perform(curl);
  if (op != CURLE_OK) {
    spdlog::error("error fetching artwork for current playing item: {}",
                  curl_easy_strerror(op));
    return nullptr;
  }
  return std::make_unique<std::vector<unsigned char>>(image);
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

// write data must be the type of std::vector<unsigned char>
size_t CurlWrite_CallbackFunc_StdBytes(void *contents, size_t size,
                                       size_t nmemb, void *userp) {
  size_t realsize = size * nmemb;
  std::vector<unsigned char> *mem = (std::vector<unsigned char> *)userp;
  size_t current_size = mem->size();
  mem->resize(current_size + realsize);
  std::memcpy(mem->data() + current_size, contents, realsize);

  return realsize;
}

std::unique_ptr<Track>
columns_to_track(const std::vector<std::string> &columns) {
  if (columns.size() != 7) {
    return nullptr;
  }

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
}

// ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠴⢶⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
// ⠀⠀⠀⠀⠀⠀⠀⠀⠀⣄⣀⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢎⡃⢻⣫⡂⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
// ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⣿⣿⣟⡶⣤⣀⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣜⢢⡾⡧⢣⢻⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀
// ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢹⣯⣷⣅⠂⠊⠛⣷⣤⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣸⡜⡘⣰⡌⣧⣷⣳⡀⠀⠀⠀⠀⠀⠀⠀⠀
// ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢸⣯⠻⢻⣷⡆⢠⢯⣽⣿⣷⡄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢷⠙⡿⡅⠇⣾⢻⢿⢳⠀⠀⠀⠀⠀⠀⠀⠀
// ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣿⣄⣂⡛⢿⣿⣿⣿⢿⣿⣿⣦⢀⣠⣴⣤⣶⣦⣦⣤⣄⡀⠀⠀⣀⠀⠀⠀⠘⣌⢼⠌⣇⡆⡄⠘⡟⣯⢧⠀⠀⠀⠀⠀⠀⠀
// ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢿⡷⣬⡝⠉⠉⢛⢿⣿⣿⣿⣿⣾⢿⣿⠿⠻⢋⣙⣻⣿⣿⣿⡿⡿⢟⣿⣿⠧⢐⢟⠟⠙⠧⣷⡂⢸⠖⣟⡆⠀⠀⠀⠀⠀⠀
// ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣸⣥⠿⠑⠃⠚⠂⣴⣿⣿⣿⢟⣿⣷⣶⣿⣿⣿⣿⣿⡿⣛⣴⣿⣿⣿⣿⣿⣷⣦⣫⡈⠦⡠⠙⢧⣿⣴⣿⣾⠀⠀⠀⠀⠀⠀
// ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⣛⡦⡤⠤⢒⢠⣾⡿⠊⣩⢖⣿⣻⣿⣻⡥⣺⣿⣿⢿⣯⡽⣛⡾⣾⣟⣿⣿⣿⣝⠾⣯⢦⣩⠈⢸⣯⣏⣟⡃⠂⠀⠀⠀⠀⠀
// ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠉⢟⡕⣸⣱⣿⠏⢤⠞⣡⢾⣿⣿⣷⣗⣼⣿⣿⣿⡷⣿⣿⡜⣾⣌⠻⠻⣮⡿⣿⣿⡿⢷⣧⣮⠸⢿⣿⣿⡿⠀⠀⠀⠀⠀⠀
// ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣼⢿⣿⠛⠠⡴⠫⡾⣳⣽⣏⣾⡟⡻⣿⡿⢓⣟⣕⢿⣿⣿⡽⣱⡄⢕⡙⢎⢿⠿⣿⣷⢾⢿⣿⣿⣿⣿⡇⠀⠀⠀⠀⠀⠀
// ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⡸⣿⢟⠕⡠⡸⣑⡽⡝⣽⠿⣜⡿⡘⢵⣾⠉⡾⡙⣿⣩⣟⣿⢿⣽⣿⣕⣧⣮⡣⣣⠹⣽⣷⠝⠻⣿⣿⡟⠃⠀⠀⠀⠀⠀⠀
// ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢰⣿⣳⢯⣴⠷⢱⡜⢿⣳⡏⣵⢿⣧⣷⣼⠟⣹⢑⣟⣻⣍⣇⢷⣻⣷⢻⣿⣜⢿⢷⡐⡡⢹⣿⣞⣷⡹⣿⡝⠀⠀⠀⠀⠀⠀⠀
// ⠀⠀⠀⠀⠀⠀⠀⠀⠀⢰⣿⢡⣏⣎⣿⢧⢶⣾⣿⡺⣳⣓⣟⢸⣋⢏⠁⣳⠪⣼⡿⣿⡉⣸⣯⣽⣷⣜⣿⣮⣿⣷⡱⣧⡻⣦⣈⣿⣿⣧⠀⠀⠀⠀⠀⠀⠀
// ⠀⠀⠀⠀⠀⠀⠀⠀⢠⡿⣷⡟⣸⣿⡿⢮⣷⣿⣾⢩⣿⣷⡋⢶⣷⣾⣿⣞⣴⣿⡏⣿⢷⣾⢠⢻⣾⣿⣾⣿⣿⣿⣧⢻⣟⣿⣿⢺⣧⣿⠀⠀⠀⠀⠀⠀⠀
// ⠀⠀⠀⠀⠀⠀⠀⣰⢯⣞⣼⢥⣿⣿⣣⣽⣾⣯⣯⣿⣟⣿⣘⣺⣫⣿⣿⣿⣿⣿⣽⣝⣿⣭⣢⣢⣻⢿⣿⡭⣮⢿⢿⣯⣿⣮⣿⡚⢿⣾⡇⠀⠀⠀⠀⠀⠀
// ⠀⠀⠀⠀⠀⠀⣰⣯⡿⣿⣾⣾⣻⣽⣈⣾⣿⣿⣻⣫⣾⣿⣿⠟⠻⢹⣿⡿⣿⠿⡻⢁⡟⠛⠭⣻⣫⣿⣿⣿⣞⣯⣿⣋⡿⣿⣾⣿⣸⣏⢷⠀⠀⠀⠀⠀⠀
// ⠀⠀⠀⠀⣠⣼⢿⣿⣿⣿⣿⣿⣻⣿⣿⣿⣿⣷⣿⣾⣿⣿⣧⣆⣀⣿⡟⣰⣿⣿⣟⢿⢀⣤⣶⣾⣿⣿⣿⣿⣿⣷⣷⡛⣼⡿⣿⣾⣷⣿⣾⣇⠀⠀⠀⠀⠀
// ⠀⠀⠀⠐⠉⢀⣾⣥⣷⣿⣿⣿⣻⣿⣿⣾⣿⣿⣿⣿⣿⣿⡟⣿⡽⢣⡀⣽⠟⠁⠊⠀⠨⣳⣿⣿⣿⣿⡿⣿⣽⣿⣷⣷⣎⣷⣿⣿⣿⣯⣿⣿⡄⠀⠀⠀⠀
// ⠀⠀⠀⠀⢠⡾⢿⣿⣿⣷⣿⣿⡬⣾⣯⣿⣍⠿⣏⣮⠟⠑⠀⢀⠏⢀⡼⠏⠀⠀⠀⠀⠘⠁⠣⠿⠎⢏⠁⣢⣿⣗⣯⣿⡟⢷⣽⡿⣟⣿⣶⠿⡿⣆⠀⠀⠀
// ⠀⠀⠀⠔⠋⠀⣸⡟⣻⢺⢿⢿⣷⣿⣿⣻⣷⡐⠉⠉⠉⠁⢉⠀⠈⠈⠀⠀⠀⠀⠀⠀⠀⠈⠉⠒⠒⠓⠺⠋⣾⣿⣾⣿⣿⣾⡻⣿⣮⣿⣿⡷⣻⡺⣆⠀⠀
// ⠀⠀⠀⠀⠀⠀⠹⣿⢻⢸⣿⡬⣷⣿⣿⣯⢿⡷⡓⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣴⡿⣺⡿⣿⣿⣸⢿⣿⣿⣷⣿⣿⣟⡇⠀⠑⠀
// ⠀⠀⠀⠀⠀⠀⢸⣷⣼⠩⣽⣿⣼⣿⣿⣿⡿⣿⡛⠲⠀⠀⠀⠀⠀⠀⠃⠀⠀⠀⠀⠀⠀⠀⠀⠀⠠⡠⡽⢯⣏⣿⣿⣿⣿⣿⣸⣿⣿⣿⡿⣿⣺⠃⠀⠀⠀
// ⠀⠀⠀⠀⠀⠀⠘⣿⣾⣦⢿⣿⣿⣧⣹⣿⣿⣝⣯⡛⠭⠀⠀⠀⠀⠀⠀⠀⣀⠀⠀⠀⠀⠀⠠⠀⠑⠪⣤⣿⡾⣻⣿⣿⣿⣿⣽⣿⣿⣿⡇⢿⡏⠀⠀⠀⠀
// ⠀⠀⠀⠀⠀⠀⠀⢿⣿⣿⣿⣿⣿⣿⣷⣜⢿⣿⣿⣿⣦⡀⠀⠀⠀⠀⠉⠅⠄⠆⠂⠀⠀⠀⠀⢀⣀⣴⣷⡿⣵⡿⠟⣹⣿⣿⣿⣿⣿⣿⡇⣼⠃⠀⠀⠀⠀
// ⠀⠀⠀⠀⠀⠀⠀⢸⣏⣿⣿⣿⣿⣿⣿⣿⣿⣟⣺⣝⢯⣻⢦⣄⠀⠀⠀⠀⠀⠀⠀⠀⠀⣀⡠⣵⣿⣿⣿⣾⡿⣡⣿⣿⣿⣿⣿⣿⣿⣿⠃⠉⠀⠀⠀⠀⠀
// ⠀⠀⠀⠀⠀⠀⠀⡮⠹⣿⣿⣿⣿⣿⣿⡛⠵⠒⠩⠛⢿⣟⢷⣿⢝⡢⣀⠀⠄⢀⣠⣴⢺⢏⢭⣾⣿⡿⣿⣫⣽⣿⣿⣿⣿⣿⣿⣿⣿⣿⠀⠀⠀⠀⠀⠀⠀
// ⠀⠀⠀⠀⠀⠀⠀⠁⣼⣷⣿⣿⡟⢫⢆⠀⠀⠀⠠⠀⠑⢝⣿⣮⣻⣿⣖⣽⣯⣛⠉⠀⠂⣼⣾⣿⣿⣿⣳⣽⣿⣯⣿⣿⣿⣯⣿⣿⣽⡟⠀⠀⠀⠀⠀⠀⠀
// ⠀⠀⠀⠀⠀⠀⠀⠀⢸⣿⣿⣟⡟⠁⠀⠀⠀⠀⠀⠀⠀⠀⠹⢿⣿⣿⣻⡀⠀⠀⠀⠀⣠⣿⣿⣿⣟⣿⣿⣯⣿⣿⣿⣿⣿⣿⣿⣿⣿⡇⠀⠀⠀⠀⠀⠀⠀
// ⠀⠀⠀⠀⠀⠀⠀⠂⠼⣿⡧⠋⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠁⠹⣿⣿⣧⣳⠀⢀⠀⠀⣿⣿⡿⣿⣻⢻⣿⣿⣿⣿⣿⣿⣿⣿⢿⣿⣿⡇⠀⠀⠀⠀⠄⠀⠀
// ⠆⠀⠀⠆⠰⠀⡄⠆⣯⣿⡏⡄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠰⠀⠀⣿⣿⣸⣿⡆⠈⠰⠀⣿⣿⣶⣿⣾⡸⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡇⠀⠀⠀⠀⠀⠀⠀
// ⠀⠀⠀⠀⠀⠃⠀⠄⣿⣿⡇⠃⠀⠠⠀⠀⠀⠀⠀⠀⠀⠀⠀⠘⢸⣿⣟⣿⣿⠀⠀⠀⢨⢿⢙⢿⣿⡅⢻⣯⡟⣿⣿⣿⣿⣿⣿⣿⣿⣿⠀⠀⠀⠀⠀⠀⠀
// ⡀⠁⠀⠀⡀⠀⠀⣰⢿⣿⡷⠁⠀⠀⠀⠐⠀⠀⠀⠀⠀⠀⠀⠀⠘⣻⣿⣿⡟⡆⠐⠀⠐⡢⠀⠈⠀⠉⠙⠿⣯⣹⣿⣿⣿⣿⣷⣿⣿⣿⡀⠀⠀⠀⠀⠀⠀
// ⠀⠂⠐⠀⠐⠀⣰⣏⣿⣿⣿⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣿⡿⣿⣧⣇⠀⠀⠐⠀⠀⠀⠀⠀⠀⠀⢀⠳⣜⠿⠿⠿⠛⠉⠋⡈⠂⠀⠀⠀⠀⠀⠀
// ⠀⡂⢈⠀⠀⠔⠓⠸⠟⠟⠿⠀⠠⠀⠠⠀⠀⠀⠂⠀⠀⠀⠀⠈⠀⣿⣿⡿⣿⢿⠀⠀⠀⠀⠐⠀⠀⠀⠀⡀⠀⠰⠈⠀⠀⠀⠀⠀⠈⠄⠀⠂⠀⠀⠀⠀⠀
// ⠀⡁⠠⢀⡀⠀⠂⠁⠀⠈⢀⠀⠀⠄⠀⠀⠀⠈⠀⠀⠂⠀⠀⠐⠑⠨⠋⠁⠉⠈⠀⠀⠀⠀⠀⠀⠀⠈⠈⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠂⢈⠅⠀⠀⠀⠀⠈
// ⠠⡈⠈⠀⠈⢀⠈⡀⠀⠀⡀⢀⠨⠀⠂⡐⠂⠀⠀⠀⠀⠀⡀⠐⠀⠀⡀⠀⠀⠀⠀⠐⠀⠀⡀⡀⡈⠀⠀⠀⠀⢂⠀⠀⠀⡀⠀⠀⠂⠀⠀⠀⠀⠀⠀⠀⠀
// ⠃⢌⠈⠐⠈⠁⠁⠀⠂⠁⠈⠂⠀⠀⠁⠄⠐⠀⠠⠄⠈⠀⠀⠈⢀⠀⠀⠢⠀⠀⡀⠀⢀⠀⠀⠀⠄⠄⠀⠁⠀⢄⠀⠀⠀⠀⠀⠁⠀⠄⠀⠀⠀⠀⠀⠀⠀