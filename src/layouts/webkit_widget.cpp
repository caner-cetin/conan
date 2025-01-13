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
#include <string>
#include <webkit/WebKitSettings.h>

struct WebKitWidget::Private {
  WebKitWebView *webView{nullptr};
  GtkWidget *gtkContainer{nullptr}; // GTK container for the WebView
  GtkWidget *gtkWindow{nullptr}; // Temporary GTK window to anchor the container
  int width{0};
  int height{0};
};

WebKitWidget::WebKitWidget(QWidget *parent, const char *initial_uri)
    : QWidget(parent), d(std::make_unique<Private>()) {
  setAttribute(Qt::WA_OpaquePaintEvent);
  setAttribute(Qt::WA_NoSystemBackground);
  setAttribute(Qt::WA_TranslucentBackground);
  setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  setFixedSize(400, 210);
  initLogger();
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
  logger->debug("Initializing WebKitWidget");
  logger->debug("Environment variables set: LIBGL_ALWAYS_SOFTWARE={}, "
                "GALLIUM_DRIVER={}, QT_QPA_PLATFORM={}",
                getenv("LIBGL_ALWAYS_SOFTWARE"), getenv("GALLIUM_DRIVER"),
                getenv("QT_QPA_PLATFORM"));

  // Ensure GTK is initialized
  if (!gtk_init_check(nullptr, nullptr)) {
    logger->error("Failed to initialize GTK");
    return false;
  }

  // Create the WebView
  d->webView = WEBKIT_WEB_VIEW(g_object_ref(webkit_web_view_new()));
  if (!d->webView || !WEBKIT_IS_WEB_VIEW(d->webView)) {
    logger->error("Failed to create WebKit view");
    return false;
  }
  logger->debug("WebKit WebView created successfully");

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
    logger->error("Failed to get GDK window from GTK widget");
    return false;
  }

  // Convert GdkWindow* to WId (unsigned long long)
  WId wid =
      GDK_WINDOW_XID(gdkWindow); // Use GDK_WINDOW_HANDLE for non-X11 platforms

  // Embed the GTK container into the QWidget
  QWindow *window = QWindow::fromWinId(wid);
  if (!window) {
    logger->error("Failed to create QWindow from WId");
    return false;
  }

  QWidget *container = QWidget::createWindowContainer(window, this);
  if (!container) {
    logger->error("Failed to create container widget");
    return false;
  }
  container->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  container->setStyleSheet("background: transparent;");
  container->setGeometry(0, 0, width(), height());
  container->setFixedSize(width(), height());
  logger->debug("Container created with geometry: {}x{} at ({},{})",
                container->width(), container->height(), container->x(),
                container->y());
  container->setUpdatesEnabled(true);
  // Configure WebKit settings
  WebKitSettings *settings = webkit_web_view_get_settings(d->webView);
  webkit_settings_set_enable_javascript(settings, TRUE);
  webkit_settings_set_enable_webgl(settings, TRUE);
  webkit_settings_set_hardware_acceleration_policy(
      settings, WEBKIT_HARDWARE_ACCELERATION_POLICY_ALWAYS);

  GdkRGBA background = {0.0, 0.0, 0.0, 0.0};
  webkit_web_view_set_background_color(d->webView, &background);
  gtk_widget_set_app_paintable(GTK_WIDGET(d->webView), FALSE);
  logger->debug("Configured WebKit settings");
  updateWebViewSize();
  return true;
}

void WebKitWidget::updateWebViewSize() {
  if (!d->webView)
    return;

  GtkWidget *widget = GTK_WIDGET(d->webView);
  int w = std::max(width(), 1) * devicePixelRatio();  // Prevent 0 width
  int h = std::max(height(), 1) * devicePixelRatio(); // Prevent 0 height

  logger->debug("Updating size: widget dimensions {}x{}, devicePixelRatio: {}",
                width(), height(), devicePixelRatio());

  if (w != d->width || h != d->height) {
    d->width = w;
    d->height = h;
    gtk_widget_set_size_request(widget, w, h);

    // Ensure container maintains size
    if (QWidget *container = findChild<QWidget *>()) {
      container->setMinimumSize(width(), height());
      container->resize(width(), height());
      logger->debug("Container dimensions after resize: {}x{}",
                    container->width(), container->height());
    }
  }
}

void WebKitWidget::resizeEvent(QResizeEvent *event) {
  // Prevent invalid resize events
  if (event->size().width() <= 0 || event->size().height() <= 0) {
    logger->warn("Ignoring invalid resize event: {}x{}", event->size().width(),
                 event->size().height());
    return; // Skip the invalid resize
  }

  logger->debug("Resize event triggered - old: {}x{}, new: {}x{}",
                event->oldSize().width(), event->oldSize().height(),
                event->size().width(), event->size().height());

  QWidget::resizeEvent(event);
  updateWebViewSize();
}
void WebKitWidget::loadURL(const QString &url, bool retry) {
  if (url.isEmpty()) {
    logger->warn("url is empty, cannot load");
    return;
  }

  // Reset retry count when loading a new URL
  if (url != pending_url) {
    load_uri_retry_count = 0;
    if (retry_timer) {
      retry_timer->stop();
    }
  }

  logger->debug("navigating to {}", url.toStdString());

  if (d->webView) {
    pending_url = url;
    if (retry) {
      g_signal_connect(d->webView, "load-changed", G_CALLBACK(load_changed_cb),
                       this);
    }
    webkit_web_view_load_uri(d->webView, url.toStdString().c_str());
  }
}

void WebKitWidget::loadHTML(const QString &html, const QString &baseURL) {
  if (d->webView) {
    webkit_web_view_load_html(d->webView, html.toUtf8().constData(),
                              baseURL.toUtf8().constData());
  }
}

void WebKitWidget::load_changed_cb(WebKitWebView *web_view,
                                   WebKitLoadEvent load_event,
                                   gpointer user_data) {
  WebKitWidget *self = static_cast<WebKitWidget *>(user_data);
  auto log = self->logger;
  switch (load_event) {
  case WEBKIT_LOAD_STARTED: {
    log->debug("Load started - establishing connection");
    break;
  }

  case WEBKIT_LOAD_REDIRECTED: {
    log->debug("Load redirected - following server redirect");
    // Reset retry count on redirect as it's a new URL
    self->load_uri_retry_count = 0;
    break;
  }

  case WEBKIT_LOAD_COMMITTED: {
    log->debug("Load committed - receiving content");
    WebKitWebResource *resource = webkit_web_view_get_main_resource(web_view);
    if (resource) {
      WebKitURIResponse *response = webkit_web_resource_get_response(resource);
      const gchar *uri = webkit_uri_response_get_uri(response);
      guint status_code = webkit_uri_response_get_status_code(response);
      log->debug("Resource committed with status code: {} for URI: {}",
                 status_code, uri);
    }
    break;
  }

  case WEBKIT_LOAD_FINISHED: {
    WebKitWebResource *resource = webkit_web_view_get_main_resource(web_view);
    if (!resource) {
      log->error("Load finished but no main resource available");
      self->scheduleRetry();
      break;
    }

    WebKitURIResponse *response = webkit_web_resource_get_response(resource);
    if (!response) {
      log->error("Load finished but no response available");
      self->scheduleRetry();
      break;
    }

    const gchar *uri = webkit_uri_response_get_uri(response);
    guint status_code = webkit_uri_response_get_status_code(response);

    log->debug("Load finished with status code: {} for URI: {}", status_code,
               uri);

    // Check if status code indicates a failure that we should retry
    if ((status_code >= 500 || status_code == 0) &&
        self->load_uri_retry_count < MAX_RETRIES) {
      log->warn("Server error (status {}), attempting retry", status_code);
      self->scheduleRetry();
    } else if (status_code >= 200 && status_code < 400) {
      log->debug("Load completed successfully");
      self->load_uri_retry_count = 0;
      self->pending_url.clear();
      if (self->retry_timer) {
        self->retry_timer->stop();
      }
    } else {
      log->error("Load failed with non-retryable status: {}", status_code);
      self->load_uri_retry_count = 0;
      self->pending_url.clear();
      if (self->retry_timer) {
        self->retry_timer->stop();
      }
    }
    break;
  }
  }
}

void WebKitWidget::scheduleRetry() {
  load_uri_retry_count++;
  logger->warn("Scheduling retry attempt {}/{}", load_uri_retry_count,
               MAX_RETRIES);

  if (!retry_timer) {
    retry_timer = new QTimer(this);
    retry_timer->setSingleShot(true);
    connect(retry_timer, &QTimer::timeout, this, [this]() {
      if (!pending_url.isEmpty()) {
        logger->debug("Executing retry for URL: {}", pending_url.toStdString());
        webkit_web_view_load_uri(d->webView, pending_url.toStdString().c_str());
      }
    });
  }

  retry_timer->start(1000); // 1 second delay
}

// ⠀⠀⠀⠀⠀⢀⠄⠄⠀⢀⠎⠀⠠⢀⡴⢣⠗⠀⠀⡌⠀⢀⡞⠁⡌⠀⠀⠀⠀⠀⡀⠀⠈⠢⡄⢠⠐⣀⢂⡑⢆⠀⠀⢀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
// ⠀⠀⠀⠀⡠⢡⠊⠀⢀⡞⠠⠒⠉⠔⢠⠇⠀⠀⡔⢀⠄⣾⠁⠀⡇⠀⠄⠂⡐⠠⠈⢦⠁⠄⠙⣆⠒⡄⢢⠒⡄⠳⡀⠀⠡⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
// ⠀⠀⠀⡠⢠⡆⢀⠄⣸⠀⠀⠀⡰⢀⡟⠀⠀⢰⠇⡌⣸⡅⠐⡀⡇⢈⠘⣆⣀⣁⠦⣄⢣⢎⡱⢤⢳⡌⡱⡜⣠⢃⠱⡄⠀⠀⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
// ⠀⠀⢰⢒⠋⡠⠀⢠⠇⠀⠀⠀⠄⣼⡇⠀⠀⡛⢠⢁⣧⢃⣰⢲⡗⣎⡛⣼⣵⡪⡕⣎⢧⢎⡱⢎⡲⢹⡴⢱⡆⡜⢢⠹⣤⡀⠀⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
// ⠀⢠⣓⡎⠠⠁⠀⣼⠀⠀⠀⡘⢀⡿⠆⠀⣀⠥⣎⢸⣽⢪⡱⢻⣿⡸⣵⡲⣹⣷⡝⡴⣋⢧⡝⢦⡙⢦⡹⣧⢹⡔⢣⠜⣳⡌⢦⢄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
// ⠀⡰⣸⠃⠀⠀⢠⡟⠀⠀⢀⡃⣸⢷⡇⢪⡑⢎⡧⣿⡎⣇⠳⣽⣿⡱⢯⣷⣧⢻⣿⣧⡝⢮⠽⣖⡹⢦⡙⣮⢷⣛⠦⡙⢼⣿⠸⣈⠆⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
// ⢀⢳⢹⠀⠂⠀⢸⡇⠀⣠⢴⡊⣿⣾⠡⢇⡙⣆⡷⣿⡟⣬⢓⣿⡇⢻⡝⣾⡌⢳⡞⣿⣿⣮⢹⡹⢧⡧⡹⢬⣯⢿⣧⡙⠦⣿⣧⢷⢨⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
// ⠈⡌⢸⠀⠆⠀⣿⢱⡉⢖⣼⠱⣿⣿⢸⡡⢏⣼⡧⣿⢳⡬⣳⣿⡇⠘⣾⣹⣳⠀⠙⢮⢿⢿⣧⣯⡙⡷⣝⠲⢮⢿⡻⣿⡥⣻⣿⢸⣉⡆⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
// ⢼⠃⢼⠀⢇⣼⣿⢠⢛⡟⣘⠧⣇⣿⢤⠻⣜⣸⡇⣿⢧⣸⣧⣿⠀⠀⠸⣇⣿⡇⠀⠀⠻⢣⡘⢿⣇⣻⠻⣟⡜⢿⣻⡻⣿⣻⣿⣸⢠⢟⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
// ⣸⠀⣾⣘⢧⢺⣿⢎⢧⡟⣧⢫⣿⣿⢮⡝⡦⡽⡗⣿⡺⣼⢾⣏⠂⠀⠀⠙⣮⣻⡀⠀⠀⠀⠙⢗⡹⠻⣷⣝⣿⣦⢿⣿⣾⡿⣿⡿⣇⢾⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
// ⢹⢲⣽⡞⣬⢻⣟⢮⢾⣓⣯⢳⣿⣿⢺⣜⡳⡵⣏⣿⣗⣯⣿⠃⠀⠀⣀⣀⣬⢷⠿⡄⠀⠀⠀⠀⠈⠙⠶⣻⡻⣿⣷⣵⠟⣿⣼⢿⡇⠞⣃⠀⠀⠀⠀⠠⠀⠀⠀⠀⠀⠀⠀⠀⠀
// ⢸⡳⢾⣟⣵⢻⣿⣎⢿⣧⢻⡽⣿⣿⣏⢾⣱⢏⡿⣿⣿⣿⡇⠒⠉⠁⠀⠀⠀⠀⠙⢽⢄⠀⠀⠀⠀⠀⠀⠀⠈⠊⠙⠿⣻⡾⢿⣿⣿⣺⣧⡤⠄⠒⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
// ⢸⡟⣽⣿⡺⣽⣿⢮⣻⣾⣹⣿⣟⣿⣯⢷⣫⢞⣿⢿⣿⡏⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠙⠱⡄⡀⠀⠀⠀⠀⢀⠀⢀⣤⣴⣤⠼⣿⣯⠍⢹⡕⢦⢄⡀⠀⠀⠀⠀⠀⠀⠀⣀⡴⠁
// ⠸⣿⣻⣿⣽⢾⣿⣏⣿⣞⡷⣯⡿⢿⣟⣮⢷⣫⢾⣿⡏⠀⠀⠀⠀⠀⠀⠀⠀⠀⡄⠀⠀⠀⠀⠉⠂⠢⠄⠀⣠⠞⠋⠁⠀⠀⢸⣿⢻⠄⠘⡇⠀⠈⠈⠚⠒⠶⠠⠰⠖⠛⠁⠀⠀
// ⠀⢻⣯⣿⣾⢻⣿⣞⣿⣽⣻⣽⠃⡀⣘⡿⣞⣵⣫⣿⡇⢀⣀⣤⡶⠾⠟⠛⠓⠒⠦⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢸⣿⣹⡎⠐⣧⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
// ⠀⠸⣿⣿⣯⢻⣿⣞⣿⣿⣳⡿⠀⢇⠀⢟⡿⣖⡧⢿⣿⠀⠊⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣸⣿⣧⠣⠀⣾⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
// ⠀⠀⢹⣿⣯⢻⣿⡞⣿⣯⢷⣷⡀⠘⠄⠼⣿⡽⣞⢯⣿⡆⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣿⣿⠜⠀⠀⡗⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
// ⠀⠀⠈⣿⣿⣹⣿⡽⣿⣏⣿⣿⣿⣄⠣⠈⣫⣿⢞⣳⢞⡇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠰⣿⣟⣷⡆⠀⡟⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
// ⠀⠀⠀⢸⣿⣹⣿⣾⡟⣾⣿⣿⣿⣿⣷⣦⣈⣻⣯⡗⣯⢷⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣾⡿⡾⣿⡆⠀⣧⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
// ⠀⠀⠀⠘⣿⣽⣿⡿⣹⣿⣿⣿⣿⣿⣿⣿⣿⣿⣞⣿⣎⡟⡆⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⡠⠄⡀⢦⣖⠆⠀⠀⠀⢀⣾⣿⣳⡇⠹⣿⣀⡿⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
// ⠀⠀⠀⠀⡿⣼⣿⢳⣿⣿⣿⡿⢿⣻⠿⣟⢯⢿⡽⣟⣾⡝⣇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠘⠕⠒⠉⠁⢂⠏⠀⠀⠀⣠⣾⣿⢯⣿⡇⠃⠘⢿⡇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
// ⠀⠀⠀⢠⢓⣿⣳⡿⣹⠋⣥⣾⠯⠉⠛⣼⢫⡞⣷⡻⢽⣿⢺⡕⠀⢀⠀⠀⠀⠀⠀⠀⠀⠀⠈⠀⠈⠀⠀⠀⢠⣼⣿⣿⡿⣽⣿⠇⠀⠀⣸⠿⣄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
// ⠀⠀⠀⡸⣸⡽⢿⠜⢁⠞⠉⠐⠀⠀⠀⢭⡳⣝⢶⢻⢈⢿⣯⢧⠀⠀⠀⠑⠀⠀⡀⠀⠀⠀⠀⠀⠀⠀⣠⡼⣏⡟⡿⣿⡗⣿⣿⡄⠀⠀⣿⠀⠙⢦⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
// ⢀⠀⢐⡽⢋⢤⠊⢠⠊⠀⠀⠀⠀⠀⠀⣺⡱⣟⠮⡏⠀⠐⢻⣿⠆⠀⠀⠀⠀⠀⠀⠀⢸⣶⡦⣤⡤⣞⣵⡻⣼⣹⢳⣿⣹⡿⢿⡇⠀⢠⡟⠀⠀⠀⠈⠢⢀⡀⠀⠀⠀⠀⡀⠀⠀
// ⠀⢠⡎⡔⢡⡞⠀⠊⠀⠀⠀⠀⠀⠀⢠⡏⣷⢩⣾⣧⠀⠀⠀⢹⣷⠀⠀⠀⠀⠀⠀⠀⣾⣧⡟⢳⣿⢱⡞⢳⣧⡏⣷⣿⣼⢸⡇⠑⣴⢺⠁⠀⠀⠀⠀⠀⠀⠀⠉⠐⠊⠁⠀⠀⠀
// ⡞⢃⡄⡓⡌⣱⡘⣍⠀⠀⠀⠀⠀⠀⣮⣝⣶⣿⣿⣿⣷⣤⣀⠀⢿⡇⠀⠀⠀⠀⠀⠀⡛⢾⣽⣟⡾⢯⣽⢻⣼⡹⠃⣿⢼⠀⢀⠀⠀⠙⠬⢆⣀⠀⠀⠀⠀⠀⠀⠀⠠⠀⠀⠀⠀
// ⡜⢦⡘⢥⠒⡤⠱⡈⢄⠀⠀⢁⠂⢠⡿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣤⣤⣄⣀⣤⣤⣷⣿⣿⣿⣿⣏⡾⣏⡶⡇⣼⢿⡾⠀⠠⠘⢶⣤⢄⠰⡟⠒⠴⠠⠀⠀⠀⠀⠁⠀⠀⠀⠀
// ⠊⠁⠙⠀⠛⢶⢣⠽⣄⠳⣄⢀⢪⠏⠳⣻⣿⣿⣿⣿⣿⣿⣽⣿⣿⣾⢿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣷⣯⣷⡿⣽⠈⣷⡇⠐⡀⠚⠹⠉⡼⠃⠀⢀⠀⠃⠀⠀⠀⠀⠀⠀⠀⡯
// ⠀⠀⠀⠀⠀⠀⠈⠳⡜⣢⡈⢗⡎⠀⠀⣷⡹⣿⣿⣟⣻⢿⣿⢿⣾⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡽⡄⠸⣽⡀⠄⠠⣠⡼⠀⡐⢀⠂⠜⠀⠀⠀⠀⠀⠀⠀⣚⡯
// ⡤⢤⣤⡤⣤⡤⢤⡤⢼⡑⣞⣲⠤⡀⠀⡏⠳⡭⢿⣿⣿⡿⣿⠿⣿⢯⣿⣿⠿⡿⠿⡿⢿⠿⣿⢿⣻⣟⣿⣿⣿⣿⣿⣳⡀⠹⣷⡈⢸⠟⠀⡐⢌⠦⣼⢄⠀⠀⢀⠀⠒⠒⠀⣿⠁
// ⣝⡫⡶⠝⠒⠩⠷⣲⢌⡙⠢⠻⠵⣮⣑⠤⡀⠈⢻⢿⣧⣷⣧⣿⣼⢸⣿⣶⣯⣷⣿⣿⣿⣿⣿⣿⢿⡿⣿⣿⢿⣿⣿⣿⣳⢡⠚⣿⣮⣄⠂⡁⢮⣿⢳⣯⢄⠂⠀⠀⠂⠀⠀⠀⠀
// ⠊⠀⢀⠠⢀⠀⠀⠀⠉⠙⠦⢄⡀⠀⠉⠳⢮⣑⠄⡙⢿⣿⣹⣏⡯⠘⣸⠯⣽⣭⣻⣽⣿⣷⣿⣾⣽⣾⣿⣿⣿⣿⣿⣿⣯⣧⢫⡿⠙⢻⢽⣲⠿⢾⠛⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
// ⠐⡈⢀⠂⠄⡈⠐⠠⢀⠀⠄⠀⢡⡀⠩⣻⡆⠉⢻⣦⣉⠻⣿⣿⠃⣡⣿⣿⠿⡿⢿⢿⣛⡟⣻⢻⣝⣯⣿⣳⣯⣾⣿⣿⣿⣗⣿⣽⡀⢧⢲⣯⠟⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀