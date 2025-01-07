#include "webkit_widget.h"
#include "spdlog/spdlog.h"
#include <QDebug>
#include <QPainter>
#include <QResizeEvent>
#include <QTimer>
#include <cairo/cairo.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <qlogging.h>
#include <qobjectdefs.h>
#include <qsizepolicy.h>
#include <qwindow.h>
#include <webkit/WebKitSettings.h>

struct WebKitWidget::Private {
  WebKitWebView *webView{nullptr};
  GtkWidget *gtkContainer{nullptr}; // GTK container for the WebView
  GtkWidget *gtkWindow{nullptr}; // Temporary GTK window to anchor the container
  int width{0};
  int height{0};
};

WebKitWidget::WebKitWidget(QWidget *parent)
    : QWidget(parent), d(std::make_unique<Private>()) {
  setAttribute(Qt::WA_OpaquePaintEvent);
  setAttribute(Qt::WA_NoSystemBackground);
  setAttribute(Qt::WA_TranslucentBackground);
  setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  setFixedSize(400, 210);
  initialize();
}

WebKitWidget::~WebKitWidget() {
  if (d->webView) {
    g_object_unref(d->webView);
  }
  if (d->gtkWindow) {
    gtk_widget_destroy(d->gtkWindow);
  }
}

bool WebKitWidget::initialize() {
  spdlog::debug("Initializing WebKitWidget");
  spdlog::debug("Environment variables set: LIBGL_ALWAYS_SOFTWARE={}, "
                "GALLIUM_DRIVER={}, QT_QPA_PLATFORM={}",
                getenv("LIBGL_ALWAYS_SOFTWARE"), getenv("GALLIUM_DRIVER"),
                getenv("QT_QPA_PLATFORM"));
  setenv("WEBKIT_DISABLE_COMPOSITING_MODE", "1", 1);

  // Ensure GTK is initialized
  if (!gtk_init_check(nullptr, nullptr)) {
    spdlog::error("Failed to initialize GTK");
    return false;
  }
  qDebug() << "Initialized GTK";

  // Create the WebView
  d->webView = WEBKIT_WEB_VIEW(g_object_ref(webkit_web_view_new()));
  if (!d->webView || !WEBKIT_IS_WEB_VIEW(d->webView)) {
    spdlog::error("Failed to create WebKit view");
    return false;
  }
  spdlog::debug("WebKit WebView created successfully");

  // Create a GTK container for the WebView
  d->gtkContainer = gtk_fixed_new();
  gtk_container_add(GTK_CONTAINER(d->gtkContainer), GTK_WIDGET(d->webView));

  // Create a temporary GTK window to anchor the container
  d->gtkWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_decorated(GTK_WINDOW(d->gtkWindow), FALSE);
  gtk_window_set_resizable(GTK_WINDOW(d->gtkWindow), FALSE);
  gtk_container_add(GTK_CONTAINER(d->gtkWindow), d->gtkContainer);
  gtk_widget_show_all(d->gtkWindow);

  // Ensure the GTK widget is realized (i.e., its window is created)
  if (!gtk_widget_get_realized(d->gtkContainer)) {
    gtk_widget_realize(d->gtkContainer);
  }

  // Get the native window ID (WId) from the GTK widget
  GdkWindow *gdkWindow = gtk_widget_get_window(d->gtkContainer);
  if (!gdkWindow) {
    spdlog::error("Failed to get GDK window from GTK widget");
    return false;
  }

  // Convert GdkWindow* to WId (unsigned long long)
  WId wid =
      GDK_WINDOW_XID(gdkWindow); // Use GDK_WINDOW_HANDLE for non-X11 platforms

  // Embed the GTK container into the QWidget
  QWindow *window = QWindow::fromWinId(wid);
  if (!window) {
    spdlog::error("Failed to create QWindow from WId");
    return false;
  }

  QWidget *container = QWidget::createWindowContainer(window, this);
  if (!container) {
    spdlog::error("Failed to create container widget");
    return false;
  }
  container->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  container->setStyleSheet("background: transparent;");
  container->setGeometry(0, 0, width(), height());
  container->setFixedSize(width(), height());
  spdlog::debug("Container created with geometry: {}x{} at ({},{})",
                container->width(), container->height(), container->x(),
                container->y());

  // Configure WebKit settings
  WebKitSettings *settings = webkit_web_view_get_settings(d->webView);
  webkit_settings_set_enable_javascript(settings, TRUE);
  webkit_settings_set_enable_webgl(settings, FALSE);
  webkit_settings_set_hardware_acceleration_policy(
      settings, WEBKIT_HARDWARE_ACCELERATION_POLICY_NEVER);

  GdkRGBA background = {0.0, 0.0, 0.0, 0.0};
  webkit_web_view_set_background_color(d->webView, &background);
  gtk_widget_set_app_paintable(GTK_WIDGET(d->webView), TRUE);
  spdlog::debug("Configured WebKit settings");
  updateWebViewSize();
  return true;
}

void WebKitWidget::updateWebViewSize() {
  if (!d->webView)
    return;

  GtkWidget *widget = GTK_WIDGET(d->webView);
  int w = std::max(width(), 1) * devicePixelRatio();  // Prevent 0 width
  int h = std::max(height(), 1) * devicePixelRatio(); // Prevent 0 height

  spdlog::debug("Updating size: widget dimensions {}x{}, devicePixelRatio: {}",
                width(), height(), devicePixelRatio());

  if (w != d->width || h != d->height) {
    d->width = w;
    d->height = h;
    gtk_widget_set_size_request(widget, w, h);

    // Ensure container maintains size
    if (QWidget *container = findChild<QWidget *>()) {
      container->setMinimumSize(width(), height());
      container->resize(width(), height());
      spdlog::debug("Container dimensions after resize: {}x{}",
                    container->width(), container->height());
    }
  }
}

void WebKitWidget::resizeEvent(QResizeEvent *event) {
  // Prevent invalid resize events
  if (event->size().width() <= 0 || event->size().height() <= 0) {
    spdlog::warn("Ignoring invalid resize event: {}x{}", event->size().width(),
                 event->size().height());
    return; // Skip the invalid resize
  }

  spdlog::debug("Resize event triggered - old: {}x{}, new: {}x{}",
                event->oldSize().width(), event->oldSize().height(),
                event->size().width(), event->size().height());

  QWidget::resizeEvent(event);
  updateWebViewSize();
}
void WebKitWidget::loadURL(const QString &url) {
  if (d->webView) {
    webkit_web_view_load_uri(d->webView, url.toUtf8().constData());
  }
}

void WebKitWidget::loadHTML(const QString &html, const QString &baseURL) {
  if (d->webView) {
    webkit_web_view_load_html(d->webView, html.toUtf8().constData(),
                              baseURL.toUtf8().constData());
  }
}