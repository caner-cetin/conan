#include "f2k.h"
#include "QWindow"
#include "icons/play.h"
#include "icons/skip.h"
#include "icons/stop.h"
#include "no_cover_art.h"
#include <QBuffer>
#include <QMovie>
#include <QSvgRenderer>

#include "conan_util.h"
#include <gdk/gdkx.h>
#include <qabstractbutton.h>
#include <qboxlayout.h>
#include <qcolor.h>
#include <qicon.h>
#include <qlistwidget.h>
#include <qmovie.h>
#include <qpushbutton.h>
#include <qsize.h>
#include <qtextedit.h>
#include <qurl.h>
#include <qwidget.h>
#include <qwindowdefs.h>

#include <gtk/gtk.h>
#include <spdlog/spdlog.h>
#include <webkit2/webkit2.h>

PlaybackControlsLayout::PlaybackControlsLayout(QWidget *parent)
    : QHBoxLayout(parent) {
  setContentsMargins(0, 0, 0, 0);
  setSpacing(5);
  play_pause = new QPushButton(parent);
  play_pause->setFixedSize(32, 32);
  play_pause->setText("▶");

  skip = new QPushButton(parent);
  skip->setFixedSize(32, 32);
  skip->setText("⏭");

  stop = new QPushButton(parent);
  stop->setFixedSize(32, 32);
  stop->setText("⏹");

  QSize icon_size(24, 24);
  play_pause->setIconSize(icon_size);
  play_pause->setIcon(hex_to_icon(Resources::PlayIcon::decompress().data(),
                                  Resources::PlayIcon::original_size, "SVG"));
  skip->setIcon(hex_to_icon(Resources::SkipIcon::decompress().data(),
                            Resources::SkipIcon::original_size, "SVG"));
  skip->setIconSize(icon_size);
  stop->setIcon(hex_to_icon(Resources::StopIcon::decompress().data(),
                            Resources::StopIcon::original_size, "SVG"));
  stop->setIconSize(icon_size);
  group = new QButtonGroup(parent);
  group->addButton(play_pause);
  group->addButton(skip);
  group->addButton(stop);
  addWidget(play_pause);
  addWidget(skip);
  addWidget(stop);
}

CoverArtLabel::CoverArtLabel(QWidget *parent) : QLabel(parent) {
  setFixedSize(QSize(180, 180));
  setAlignment(Qt::AlignCenter);
  setScaledContents(false);

  auto placeholder_gif =
      hex_to_byte(Resources::NoCoverArtGif::decompress().data(),
                  Resources::NoCoverArtGif::original_size);
  placeholder_buffer = new QBuffer(this);
  placeholder_buffer->setData(placeholder_gif);
  placeholder_buffer->open(QIODevice::ReadOnly);

  placeholder = new QMovie(parent);
  placeholder->setDevice(placeholder_buffer);
  placeholder->setScaledSize(QSize(180, 180));
  placeholder->start();

  setMovie(placeholder);
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

void CoverArtLabel::on_cover_art_change(std::vector<unsigned char> art) {
  if (art.size() > 0) {
    if (placeholder->state() == QMovie::Running) {
      placeholder->stop();
    }
    clear();
    QImage img;
    if (!img.loadFromData(art.data(), art.size())) {
      spdlog::error("cannot load cover art");
      return;
    }
    QPixmap pix = QPixmap::fromImage(img);
    setPixmap(
        pix.scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
  } else {
    clear();
    setMovie(placeholder);
    placeholder->start();
  }
}

TrackAnalysisInfo::TrackAnalysisInfo(QWidget *parent) : QWidget(parent) {}

TrackListFilter::TrackListFilter(QWidget *parent) : QTextEdit(parent) {
  setMaximumHeight(30);
}

TrackList::TrackList(QWidget *parent) : QListWidget(parent) {}