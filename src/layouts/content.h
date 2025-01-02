#ifndef CONTENT_H
#define CONTENT_H

#include <QHBoxLayout>
#include <QTextEdit>
#include <QVBoxLayout>

class ContentLayout : public QHBoxLayout {
  Q_OBJECT

 public:
  ContentLayout(QWidget* parent = nullptr);

 private:
  QTextEdit* trackFilter;
  QVBoxLayout* trackList;
};

#endif  // CONTENT_H