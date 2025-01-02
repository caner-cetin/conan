#include "content.h"

ContentLayout::ContentLayout(QWidget* parent) : QHBoxLayout(parent) {
  trackFilter = new QTextEdit(parent);
  trackList = new QVBoxLayout();

  this->addLayout(trackList);
  this->addWidget(trackFilter);

  trackFilter->setMaximumHeight(30);

  if (parent) {
    parent->setLayout(this);
  }
}