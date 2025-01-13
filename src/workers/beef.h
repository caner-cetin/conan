#ifndef BEEF_H
#define BEEF_H

#include <curl/curl.h>
#include <fmt/core.h>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

using namespace nlohmann;

struct Track {
  std::string artist;
  std::string title;
  std::string album;
  std::string length;
  int8_t track_number;
  int8_t total_tracks;
  std::string path;
};
// change to array later
static std::vector<std::string> TrackQueryColumns = {
    "%artist%]",      "%title%]",       "%album%]", "%length%]",
    "%tracknumber%]", "%totaltracks%]", "%path%]",
};
std::unique_ptr<Track>
columns_to_track(const std::vector<std::string> &columns);

enum PlaybackState { Stopped, Playing, Paused, Invalid = -1 };

NLOHMANN_JSON_SERIALIZE_ENUM(PlaybackState, {{Stopped, "stopped"},
                                             {Playing, "playing"},
                                             {Paused, "paused"},
                                             {Invalid, nullptr}});
// https://stackoverflow.com/a/73360647/22757599
template <> class fmt::formatter<PlaybackState> {
public:
  constexpr auto parse(format_parse_context &ctx) { return ctx.begin(); }
  template <typename Context>
  constexpr auto format(PlaybackState const &p, Context &ctx) const {
    const char *status = "";
    switch (p) {
    case Invalid:
      status = "Player Not Available";
      break;
    case Stopped:
      status = "Stopped";
      break;
    case Playing:
      status = "Playing";
      break;
    case Paused:
      status = "Paused";
    }
    return format_to(ctx.out(), "{}", status); // --== KEY LINE ==--
  }
};
// https://editor.swagger.io/
// import from url
// https://raw.githubusercontent.com/hyperblast/beefweb/96091e9e15a32211e499f447e9112d334311bcb3/docs/player-api.yml
namespace BeefWeb {
namespace Playback {
void play_pause_toggle();
void stop();
void skip();
} // namespace Playback
namespace PlayerState { // /player
class Info {
public:
  std::string name;
  std::string title;
  std::string version;
  std::string pluginVersion;

  NLOHMANN_DEFINE_TYPE_INTRUSIVE(Info, name, title, version, pluginVersion);
};

class ActiveItem {
public:
  std::string playlistId;
  int8_t playlistIndex;
  int8_t index;
  float position;
  float duration;
  std::vector<std::string> columns;

  NLOHMANN_DEFINE_TYPE_INTRUSIVE(ActiveItem, playlistId, playlistIndex, index,
                                 position, duration, columns);

  std::unique_ptr<std::vector<unsigned char>> artwork();
};

class Volume {
public:
  std::string type;
  float min;
  float max;
  float value;
  bool isMuted;

  NLOHMANN_DEFINE_TYPE_INTRUSIVE(Volume, type, min, max, value, isMuted);
};

class Player {
public:
  Info info;
  ActiveItem activeItem;
  PlaybackState playbackState;
  Volume volume;

  NLOHMANN_DEFINE_TYPE_INTRUSIVE(Player, info, activeItem, playbackState,
                                 volume)
};

class State {
public:
  Player player;

  NLOHMANN_DEFINE_TYPE_INTRUSIVE(State, player);
};
std::unique_ptr<Player> query();
} // namespace PlayerState

namespace PlaylistItems { // # /playlists/{playlistId}/items/{range}
class Item {
public:
  // todo: change this to fixed 7 sized array
  std::vector<std::string> columns;
  NLOHMANN_DEFINE_TYPE_INTRUSIVE(Item, columns);
};
class Items {
public:
  std::vector<Item> items;
  int8_t offset;
  int8_t totalCount;
  NLOHMANN_DEFINE_TYPE_INTRUSIVE(Items, items, offset, totalCount);
};
class Playlist {
public:
  Items playlistItems;
  NLOHMANN_DEFINE_TYPE_INTRUSIVE(Playlist, playlistItems);
};
std::pair<std::unique_ptr<Track>, std::unique_ptr<Track>>
current_and_next_track(BeefWeb::PlayerState::ActiveItem *active_item);
}; // namespace PlaylistItems

} // namespace BeefWeb

#endif
