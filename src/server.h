#include "crow/websocket.h"
#include "f2k_types.h"
#include <QThread>
#include <qtmetamacros.h>
#include <string>
class HttpWorker : public QThread {
  Q_OBJECT
  void run() override;

public Q_SLOTS:
  void destroy() { quit(); };
  // void stream_playback_state(bool changed, PlaybackState state);

private:
  // there is only one client and thats us from QT
  crow::websocket::connection *client = nullptr;
  void stream_swap(const std::string &swap_for, const std::string &replacement);
};