#ifndef TOOLBOX_H
#define TOOLBOX_H

#include <QtCore/qtmetamacros.h>

#include <QHBoxLayout>
#include <QObject>
#include <QPushButton>

class ToolbarLayout : public QHBoxLayout {
  Q_OBJECT
 public:
  ToolbarLayout(QObject* parent = nullptr);

 private:
  QPushButton* selectDirectory = new QPushButton("Select Music Directory");
  QPushButton* loadAnalysis = new QPushButton("Load Analysis");
  QPushButton* saveAnalysis = new QPushButton("Save Analysis");
  QPushButton* startAnalysis = new QPushButton("Start Analysis");
  QPushButton* stopAnalysis = new QPushButton("Stop Analysis");
};

#endif