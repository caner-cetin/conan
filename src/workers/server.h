#include "crow/app.h"
#include "crow/websocket.h"
#include "workers/beef.h"
#include <QMutex>
#include <QThread>
#include <memory>
#include <qapplication.h>
#include <qobject.h>
#include <qtmetamacros.h>
class HttpWorker : public QThread {
  Q_OBJECT
  void run() override;

public:
  HttpWorker(QObject *parent = nullptr);
  ~HttpWorker() {
    server.stop();
    if (isRunning()) {
      quit();
      wait();
    }
  }
public Q_SLOTS:
  void update_player_state();
  void handle_play_pause_button_event();
  void handle_skip_button_event();
  void handle_stop_button_event();
Q_SIGNALS:
  void cover_art_changed(std::vector<unsigned char> data);

private:
  std::mutex mutex;
  crow::SimpleApp server;
  crow::websocket::connection *client;
  QTimer *player_poll;
  std::unique_ptr<BeefWeb::PlayerState::Player> current_player_state;
  std::unique_ptr<Track> currently_playing;
  std::unique_ptr<Track> up_next;

  void broadcast_player_state();
  void broadcast_up_next(bool clear = false);
  void broadcast_currently_playing(bool clear = false);
  void broadcast_all_properties();
  void fetch_and_broadcast_current_and_up_next_track();

  bool up_next_clear = true;
  bool currently_playing_clear = true;
};
