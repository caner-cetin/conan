#ifndef TOOLBAR_H
#define TOOLBAR_H

#include "workers/analysis.h"
#include <QtCore/qtmetamacros.h>

#include <QHBoxLayout>
#include <QObject>
#include <QPushButton>

class ToolbarLayout : public QHBoxLayout {
  Q_OBJECT
public:
  ToolbarLayout(QObject *parent = nullptr);
  QString music_directory;

private:
  QPushButton *select_directory = new QPushButton("Select Music Directory");
  QPushButton *load_analysis = new QPushButton("Load Analysis");
  QPushButton *save_analysis = new QPushButton("Save Analysis");
  QPushButton *start_analysis = new QPushButton("Start Analysis");
  QPushButton *stop_analysis = new QPushButton("Stop Analysis");

  MusicAnalyzer analyzer;

public Q_SLOTS:
  void on_select_directory(bool checked = false);
  void on_load_analysis(bool checked = false);
  void on_save_analysis(bool checked = false);
  void on_start_analysis(bool checked = false);
  void on_stop_analysis(bool checked = false);
};

#endif