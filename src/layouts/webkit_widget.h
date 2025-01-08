#ifndef WEBKIT_WIDGET_H
#define WEBKIT_WIDGET_H
#include <QImage>
#include <QWidget>
#include <memory>
#include <qcontainerfwd.h>
#include <webkit2/webkit2.h>

class WebKitWidget : public QWidget {
  Q_OBJECT
public:
  explicit WebKitWidget(QWidget *parent = nullptr,
                        const char *initial_uri = nullptr);
  ~WebKitWidget();

  bool initialize();
  void loadURL(const QString &url);
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
};
#endif