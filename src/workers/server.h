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

private:
  std::mutex mutex;
  crow::SimpleApp server;
  crow::websocket::connection *client;
  QTimer *player_poll;
  std::unique_ptr<BeefWeb::PlayerState::Player> current_player_state;
  std::unique_ptr<Track> currently_playing;
  std::unique_ptr<Track> up_next;

  void broadcast_player_state();
  void broadcast_up_next();
  void broadcast_currently_playing();
  void broadcast_all_properties();
  void fetch_and_broadcast_current_and_up_next_track();
};
