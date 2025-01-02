#ifndef PROGRESS_BAR_H
#define PROGRESS_BAR_H
#include <QProgressBar>
#include <QtCore/qtmetamacros.h>
#include <QWidget>
class ProgressBar : public QProgressBar {
  public:
    ProgressBar(QWidget* parent);
};
#endif