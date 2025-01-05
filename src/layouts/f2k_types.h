#ifndef F2K_TYPES_H
#define F2K_TYPES_H
#include "string"
#include "vector"

struct ActiveTrack {
  std::string artist;
  std::string title;
  std::string album;
  std::string length;
  std::string path;
  int8_t track_number;
  int8_t total_tracks;
};

enum PlaybackState { Stopped, Playing, Paused };

namespace BeefWeb {
namespace PlayerState {
struct Info {
  std::string name;
  std::string title;
  std::string version;
  std::string pluginVersion;
};
struct ActiveItem {
  std::string *playlistId;
  std::string playlistIndex;
  int8_t index;
  float position;
  float duration;
  std::vector<std::string> columns;
};
struct Volume {
  std::string type;
  float min;
  float max;
  float value;
  bool isMuted;
};
struct Player {
  Info info;
  ActiveItem activeItem;
  PlaybackState playbackState;
  Volume volume;
};
}; // namespace PlayerState
} // namespace BeefWeb
#endif