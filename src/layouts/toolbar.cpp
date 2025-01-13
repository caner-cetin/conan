#include "toolbar.h"
#include "workers/analysis.h"
#include <QFileDialog>
#include <cstring>
#include <qfiledialog.h>
#include <qobject.h>
#include <qpushbutton.h>
#include <spdlog/spdlog.h>

ToolbarLayout::ToolbarLayout(QObject *parent) {
  connect(select_directory, &QPushButton::clicked, this,
          &ToolbarLayout::on_select_directory);
  connect(load_analysis, &QPushButton::clicked, this,
          &ToolbarLayout::on_load_analysis);
  connect(save_analysis, &QPushButton::clicked, this,
          &ToolbarLayout::on_save_analysis);
  connect(start_analysis, &QPushButton::clicked, this,
          &ToolbarLayout::on_start_analysis);
  connect(stop_analysis, &QPushButton::clicked, this,
          &ToolbarLayout::on_stop_analysis);
  this->addWidget(select_directory);
  this->addWidget(load_analysis);
  this->addWidget(save_analysis);
  this->addWidget(start_analysis);
  this->addWidget(stop_analysis);
}

void ToolbarLayout::on_select_directory(bool checked) {
  music_directory = QFileDialog::getExistingDirectory(
      this->widget(), "Select Music Directory", "/home",
      QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
  spdlog::debug("selected directory: {}", music_directory.toStdString());
}

void ToolbarLayout::on_start_analysis(bool checked) {
  if (music_directory == nullptr ||
      strcmp(music_directory.toStdString().c_str(), "") == 0) {
    return;
  }
  spdlog::debug("analyzing directory: {}", music_directory.toStdString());
  analyzer.analyze_directory(music_directory);
}

void ToolbarLayout::on_stop_analysis(bool checked) {
  // todo: stop the analysis worker
  start_analysis->setEnabled(true);
  stop_analysis->setEnabled(false);
}

void ToolbarLayout::on_save_analysis(bool checked) {
  QString savefile;
  QFileDialog::getSaveFileName(this->widget(), "Save Analysis", savefile,
                               "SQLite3 Database File (*.db)");
  // todo: save mechanism
}

void ToolbarLayout::on_load_analysis(bool checked) {
  QString savefile;
  QFileDialog::getOpenFileName(this->widget(), "Load Analysis", savefile,
                               "SQLite3 Database File (*.db)");
}