#include <assets/favicon.h>
#include <assets/stylesheet.h>

#include <layouts/toolbar.h>
#include <layouts/progress_bar.h>
#include <layouts/content.h>

#include <essentia/algorithmfactory.h>
#include <essentia/essentia.h>
#include <essentia/pool.h>
#include <fmt/core.h>

#include <QApplication>
#include <QDialog>
#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
#include <QString>
#include <QVBoxLayout>
#include <vector>
using namespace essentia;
using namespace essentia::standard;
std::string formatDuration(float duration);
int main(int argc, char** argv) {
  QApplication app(argc, argv);
  QIcon favicon = loadFavIcon();
  app.setWindowIcon(favicon);
  app.setStyleSheet(
      QString(std::string(reinterpret_cast<const char*>(Resources::stylesheet))
                  .c_str()));
  QMainWindow window;
  window.setWindowTitle("Conan");
  window.setMinimumSize(1700, 900);
  window.setWindowIcon(favicon);
  QWidget* centralWidget = new QWidget(&window);
  window.setCentralWidget(centralWidget);

  QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);

  mainLayout->addLayout(new ToolbarLayout(centralWidget));
  mainLayout->addWidget(new ProgressBar(centralWidget));
  mainLayout->addLayout(new ContentLayout(centralWidget));

  window.show();

  return app.exec();
}

std::string formatDuration(float duration) {
  int totalSeconds = static_cast<int>(duration);  // Extract total seconds
  int minutes = totalSeconds / 60;                // Calculate minutes
  int seconds = totalSeconds % 60;                // Calculate remaining seconds

  return fmt::format("{:02d}:{:02d}", minutes, seconds);
}