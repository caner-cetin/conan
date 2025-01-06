#include "f2k.h"
#include "QWindow"
#include "assets/util.h"
#include "gtk/gtk.h"
#include "icons/play.h"
#include "icons/skip.h"
#include "icons/stop.h"
#include "no_cover_art.h"
#include <QBuffer>
#include <QMovie>
#include <QSvgRenderer>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <qabstractbutton.h>
#include <qboxlayout.h>
#include <qcolor.h>
#include <qicon.h>
#include <qpushbutton.h>
#include <qsize.h>
#include <qurl.h>
#include <qwidget.h>
#include <qwindowdefs.h>
#include <webkit2/webkit2.h>
PlaybackControlsLayout::PlaybackControlsLayout(QWidget *parent) {
  play_pause = new QPushButton(parent);
  skip = new QPushButton(parent);
  stop = new QPushButton(parent);
  QSize icon_size(24, 24);
  play_pause->setIconSize(icon_size);
  play_pause->setIcon(
      hex_to_icon(Resources::PlayIcon::hex, Resources::PlayIcon::size, "SVG"));
  skip->setIcon(
      hex_to_icon(Resources::SkipIcon::hex, Resources::SkipIcon::size, "SVG"));
  skip->setIconSize(icon_size);
  stop->setIcon(
      hex_to_icon(Resources::StopIcon::hex, Resources::StopIcon::size, "SVG"));
  stop->setIconSize(icon_size);
  group = new QButtonGroup(parent);
  group->addButton(play_pause);
  group->addButton(skip);
  group->addButton(stop);
  addWidget(play_pause);
  addWidget(skip);
  addWidget(stop);

  if (parent) {
    setParent(parent);
  }
}

CoverArtLabel::CoverArtLabel(QWidget *parent) {
  setFixedSize(QSize(180, 180));
  setAlignment(Qt::AlignCenter);
  setScaledContents(false);

  auto placeholder_gif = hex_to_byte(Resources::NoCoverArtGif::hex,
                                     Resources::NoCoverArtGif::size);
  placeholder_buffer = new QBuffer(this);
  placeholder_buffer->setData(placeholder_gif);
  placeholder_buffer->open(QIODevice::ReadOnly);

  placeholder = new QMovie(parent);
  placeholder->setDevice(placeholder_buffer);
  placeholder->setScaledSize(QSize(180, 180));
  placeholder->start();

  setMovie(placeholder);

  if (parent) {
    setParent(parent);
  }
}

CoverArtLabel::~CoverArtLabel() {
  if (placeholder) {
    placeholder->stop();
    delete placeholder;
  }
  if (placeholder_buffer) {
    placeholder_buffer->close();
    delete placeholder_buffer;
  }
}
TrackPlayerInfo::TrackPlayerInfo(QWidget *parent) : QWidget(parent) {
  setMaximumHeight(200);

  // Initialize GTK
  gtk_init(nullptr, nullptr);

  // Create WebKit view
  webView = WEBKIT_WEB_VIEW(webkit_web_view_new());

  // Configure WebKit settings
  WebKitSettings *settings = webkit_web_view_get_settings(webView);
  webkit_settings_set_enable_javascript(settings, TRUE);

  // Get the GTK widget for the WebView
  GtkWidget *gtkWidget = GTK_WIDGET(webView);

  // Realize the GTK widget (create its native window)
  gtk_widget_realize(gtkWidget);

  // Get the GdkWindow from the GTK widget
  GdkWindow *window = gtk_widget_get_window(gtkWidget);
  if (!window) {
    qWarning() << "Failed to get GdkWindow";
    return;
  }

  // Get the XID from the GdkWindow
  guintptr windowId = GDK_WINDOW_XID(window);

  // Create a QWindow from the native window ID
  QWindow *qWindow = QWindow::fromWinId(windowId);

  // Create a QWidget container for the QWindow
  QWidget *container = QWidget::createWindowContainer(qWindow, parent);
  container->setMinimumSize(400, 200); // Set a minimum size for the container

  // Add the container to the layout
  auto layout = new QVBoxLayout(parent);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(container);

  // Load a URL into the WebView
  webkit_web_view_load_uri(webView, "http://localhost:31311");

  // Make the GTK widget visible
  gtk_widget_set_visible(gtkWidget, true);
}

TrackAnalysisInfo::TrackAnalysisInfo(QWidget *parent) : QWidget(parent) {}

TrackListFilter::TrackListFilter(QWidget *parent) {
  setMaximumHeight(30);

  if (parent) {
    setParent(parent);
  }
}

TrackList::TrackList(QWidget *parent) {

  if (parent) {
    setParent(parent);
  }
}