#include "toolbar.h"

ToolbarLayout::ToolbarLayout(QObject* parent) {
  this->addWidget(selectDirectory);
  this->addWidget(loadAnalysis);
  this->addWidget(saveAnalysis);
  this->addWidget(startAnalysis);
  this->addWidget(stopAnalysis);
}