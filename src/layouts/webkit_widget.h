#ifndef WEBKIT_WIDGET_H
#define WEBKIT_WIDGET_H
#include <QImage>
#include <QWidget>
#include <memory>
#include <qcontainerfwd.h>
#include <qtimer.h>
#include <spdlog/spdlog.h>
#include <webkit2/webkit2.h>

class WebKitWidget : public QWidget {
  Q_OBJECT
public:
  explicit WebKitWidget(QWidget *parent = nullptr,
                        const char *initial_uri = nullptr);
  ~WebKitWidget();

  bool initialize();
  void loadURL(const QString &url, bool retry = true);
  void loadHTML(const QString &html, const QString &baseURL = "about:blank");

protected:
  void resizeEvent(QResizeEvent *event) override;

private:
  struct Private;
  std::unique_ptr<Private> d;

  static void onSnapshotReady(WebKitWebView *webView, GAsyncResult *result,
                              void *data);
  void updateWebViewSize();
  void requestSnapshot();

  int load_uri_retry_count = 0;
  static constexpr int MAX_RETRIES = 5;
  QTimer *retry_timer = nullptr;
  QString pending_url;
  std::shared_ptr<spdlog::logger> logger;

  static void load_changed_cb(WebKitWebView *web_view,
                              WebKitLoadEvent load_event, gpointer user_data);
  void scheduleRetry();

  void initLogger() {
    logger = spdlog::default_logger()->clone("WebKitWidget");
  }
  void updateRender();
};

class RenderWidget : public QWidget {
public:
  explicit RenderWidget(QWidget *parent = nullptr) : QWidget(parent) {
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_NoSystemBackground);
  }

  void setImage(const QImage &img) {
    image = img;
    update();
  }

protected:
  void paintEvent(QPaintEvent *);

private:
  QImage image;
};

#endif