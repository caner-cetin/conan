

#include <QListWidget>
#include <qabstractbutton.h>
#include <qboxlayout.h>
#include <qbuffer.h>
#include <qbuttongroup.h>
#include <qlabel.h>
#include <qlistwidget.h>
#include <qobject.h>
#include <qobjectdefs.h>
#include <qpushbutton.h>
#include <qtextedit.h>
#include <qtmetamacros.h>
#include <qwidget.h>
#include <qwindowdefs.h>
#include <webkit2/webkit2.h>

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
  ~CoverArtLabel();

private:
  QMovie *placeholder;
  QBuffer *placeholder_buffer;
};
class TrackAnalysisInfo : public QWidget {
public:
  TrackAnalysisInfo(QWidget *parent = nullptr);

private:
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