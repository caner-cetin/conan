

#include <qabstractbutton.h>
#include <qboxlayout.h>
#include <qbuttongroup.h>
#include <qlabel.h>
#include <qlistwidget.h>
#include <qobject.h>
#include <qobjectdefs.h>
#include <qpushbutton.h>
#include <qtextedit.h>
#include <qtmetamacros.h>
#include <qwebengineview.h>
#include <qwidget.h>
#include <qwindowdefs.h>
#include <QListWidget>

class PlaybackControlsLayout : public QHBoxLayout {

public:
  PlaybackControlsLayout(QWidget *parent = nullptr);

private:
  QButtonGroup *group;
  QPushButton *play_pause;
  QPushButton *stop;
  QPushButton *skip;
};

class CoverArtLabel : public QLabel {
public:
  CoverArtLabel(QWidget *parent = nullptr);

private:
  QMovie *placeholder;
};

class TrackPlayerInfo : public QWebEngineView {
public:
  TrackPlayerInfo(QWidget *parent = nullptr);
};

class TrackAnalysisInfo : public QWebEngineView {
public:
  TrackAnalysisInfo(QWidget *parent = nullptr);
};

class TrackList : public QListWidget {
public:
  TrackList(QWidget *parent = nullptr);

private:
};

class TrackListFilter : public QTextEdit {
public:
  TrackListFilter(QWidget *parent = nullptr);

private:
};

class Foobar2k : public QObject {
  Q_OBJECT

public:
  Foobar2k(QWidget *parent);

private:
  QTimer *player_state_poll;
  QVBoxLayout player_layout;
  QLabel cover_art;
  PlaybackControlsLayout *playback_controls;

Q_SIGNALS:
  void playback_state_changed(bool ch);
  void active_track_changed(bool ch);
};
