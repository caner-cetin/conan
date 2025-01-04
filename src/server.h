#include "crow/websocket.h"
#include "f2k_types.h"
#include <QMutex>
#include <QThread>
#include <qapplication.h>
#include <qobject.h>
#include <qtmetamacros.h>
#include <string>
class HttpWorker : public QThread {
  Q_OBJECT
  void run() override;

public:
  HttpWorker(QObject *parent = nullptr);

public Q_SLOTS:
  void destroy() {
    QMutexLocker locker(&mutex);
    quit();
    wait();
  }
  // void stream_playback_state(bool changed, PlaybackState state);

private:
  QObject *parent;
  QMutex mutex;
  crow::websocket::connection *client{nullptr};
  void stream_swap(const std::string &swap_for, const std::string &replacement);
};