#include "webkit_widget.h"
#include "spdlog/spdlog.h"
#include <QDebug>
#include <QPainter>
#include <QResizeEvent>
#include <QTimer>
#include <QVBoxLayout>
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
#include <webkit2/webkit2.h>
struct WebKitWidget::Private {
  WebKitWebView *webView{nullptr};
  GtkWidget *gtkContainer{nullptr}; // GTK container for the WebView
  GtkWidget *gtkWindow{nullptr}; // Temporary GTK window to anchor the container
  RenderWidget *renderWidget{nullptr};
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

  // Initialize GTK
  gtk_init(nullptr, nullptr);

  // Create WebView
  d->webView = WEBKIT_WEB_VIEW(webkit_web_view_new());
  if (!d->webView || !WEBKIT_IS_WEB_VIEW(d->webView)) {
    logger->error("Failed to create WebKit view");
    return false;
  }

  // Create a container for the WebView using QVBoxLayout
  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  // Create an offscreen window for the WebKit view
  d->gtkWindow = gtk_offscreen_window_new();
  gtk_container_add(GTK_CONTAINER(d->gtkWindow), GTK_WIDGET(d->webView));

  // Show the WebKit widget
  gtk_widget_show_all(d->gtkWindow);
  gtk_widget_realize(d->gtkWindow);

  // Create our custom render widget
  d->renderWidget = new RenderWidget(this);
  d->renderWidget->setFixedSize(width(), height());

  // Add the render widget to our layout
  layout->addWidget(d->renderWidget);

  // Connect the GTK widget's draw signal
  g_signal_connect(
      d->gtkWindow, "damage-event",
      G_CALLBACK(+[](GtkWidget *widget, GdkEventExpose *event, gpointer data) {
        WebKitWidget *self = static_cast<WebKitWidget *>(data);
        self->updateRender();
        return TRUE;
      }),
      this);

  // Configure WebKit settings
  WebKitSettings *settings = webkit_web_view_get_settings(d->webView);
  webkit_settings_set_enable_javascript(settings, TRUE);
  webkit_settings_set_enable_webgl(settings, TRUE);

  // Set transparent background
  GdkRGBA background = {0.0, 0.0, 0.0, 0.0};
  webkit_web_view_set_background_color(d->webView, &background);

  updateWebViewSize();
  return true;
}

void WebKitWidget::updateRender() {
  if (!d->gtkWindow || !d->renderWidget)
    return;

  cairo_surface_t *surface =
      gtk_offscreen_window_get_surface(GTK_OFFSCREEN_WINDOW(d->gtkWindow));
  if (!surface)
    return;

  // Get the surface dimensions
  int width = cairo_image_surface_get_width(surface);
  int height = cairo_image_surface_get_height(surface);

  // Create a QImage from the cairo surface data
  unsigned char *data = cairo_image_surface_get_data(surface);
  QImage image(data, width, height, cairo_image_surface_get_stride(surface),
               QImage::Format_ARGB32_Premultiplied);

  // Update our render widget with the new image
  d->renderWidget->setImage(
      image.copy()); // Make a copy since the cairo data might be temporary
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

void RenderWidget::paintEvent(QPaintEvent *) {
  if (image.isNull())
    return;
  QPainter painter(this);
  painter.drawImage(rect(), image);
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