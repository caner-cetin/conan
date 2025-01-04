#include "f2k.h"
#include "assets/util.h"
#include "icons/play.h"
#include "icons/skip.h"
#include "icons/stop.h"
#include "no_cover_art.h"
#include <QBuffer>
#include <QMovie>
#include <QWebEngineSettings>
#include <memory>
#include <qabstractbutton.h>
#include <qcolor.h>
#include <qpushbutton.h>
#include <qsize.h>
#include <qurl.h>
#include <qwebenginesettings.h>
#include <qwidget.h>

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
  placeholder = new QMovie(parent);
  placeholder->setDevice(new QBuffer(&placeholder_gif));
  placeholder->setScaledSize(QSize(180, 180));
  placeholder->start();

  setMovie(placeholder);

  if (parent) {
    setParent(parent);
  }
}

TrackPlayerInfo::TrackPlayerInfo(QWidget *parent) {
  setMaximumHeight(200);
  page()->setBackgroundColor(QColor(0, 0, 0, 0));
  QWebEngineSettings *st = page()->settings();
  st->setAttribute(QWebEngineSettings::JavascriptEnabled, true);
  st->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, true);
  load(QUrl("http://localhost:31311"));

  if (parent) {
    setParent(parent);
  }
}

TrackAnalysisInfo::TrackAnalysisInfo(QWidget *parent) {
  page()->setBackgroundColor(QColor(0, 0, 0, 0));
  QWebEngineSettings *st = page()->settings();
  st->setAttribute(QWebEngineSettings::JavascriptEnabled, true);
  st->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, true);

  if (parent) {
    setParent(parent);
  }
}

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