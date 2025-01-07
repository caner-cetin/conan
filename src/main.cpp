
#include "webkit_widget.h"
#include <cstdlib>
#include <qboxlayout.h>
#define WINDOW_MIN_W 1700
#define WINDOW_MIN_H 900

#include <assets/favicon.h>
#include <assets/stylesheet.h>
#include <assets/util.h>
#include <workers/server.h>

#include <layouts/f2k.h>
#include <layouts/progress_bar.h>
#include <layouts/toolbar.h>
#include <layouts/weight_slider.h>

#include <QApplication>
#include <QDebug>
#include <QDialog>
#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
#include <QString>
#include <QVBoxLayout>
#include <essentia/algorithmfactory.h>
#include <essentia/essentia.h>
#include <essentia/pool.h>
#include <fmt/core.h>
#include <spdlog/spdlog.h>
using namespace essentia;
using namespace essentia::standard;
std::string formatDuration(float duration);
int main(int argc, char **argv) {
  setenv("GALLIUM_DRIVER", "llvmpipe", 1);
  setenv("QT_QPA_PLATFORM", "xcb", 1);
  qputenv("QT_FATAL_WARNINGS", "1");
#ifdef NDEBUG
  spdlog::set_level(spdlog::level::info);
#else
  spdlog::set_level(spdlog::level::debug);
#endif

  QApplication app(argc, argv);

  // Application setup
  auto favicon =
      hex_to_icon(Resources::Favicon::hex, Resources::Favicon::size, "ICO");
  app.setWindowIcon(favicon);
  app.setStyleSheet(Resources::Stylesheet::string);

  // Main window setup
  QMainWindow window;
  window.setWindowTitle("Conan");
  window.setMinimumSize(WINDOW_MIN_W, WINDOW_MIN_H);
  window.setWindowIcon(favicon);

  // Root layout setup
  QWidget *centralWidget = new QWidget(&window);
  window.setCentralWidget(centralWidget);
  QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

  // Top section: Toolbar and progress bar
  {
    mainLayout->addLayout(new ToolbarLayout(centralWidget));
    mainLayout->addWidget(new ProgressBar(centralWidget));
  }

  // Main content area
  QHBoxLayout *contentLayout = new QHBoxLayout();
  mainLayout->addLayout(contentLayout);

  // Left side: Main content area
  {
    QVBoxLayout *leftPanel = new QVBoxLayout();
    contentLayout->addLayout(leftPanel, 1); // 1/3 of horizontal space

    // Track list and filter
    leftPanel->addWidget(new TrackListFilter());
    leftPanel->addWidget(new TrackList());

    // Player section
    {
      QHBoxLayout *playerSection = new QHBoxLayout();
      leftPanel->addLayout(playerSection);

      // Left side of player (Cover art and controls)
      {
        QVBoxLayout *playerLeft = new QVBoxLayout();
        playerSection->addLayout(playerLeft);

        CoverArtLabel *coverArt = new CoverArtLabel();
        PlaybackControlsLayout *playbackControls = new PlaybackControlsLayout();

        playerLeft->addWidget(coverArt);
        playerLeft->addLayout(playbackControls);
      }

      // Right side of player (Track info)
      {
        QVBoxLayout *playerRight = new QVBoxLayout();
        playerSection->addLayout(playerRight);
        auto TrackPlayerInfo = new WebKitWidget();
        TrackPlayerInfo->loadURL("http://localhost:31311");
        playerRight->addWidget(TrackPlayerInfo);
      }
    }

    // Weight sliders at bottom
    leftPanel->addWidget(new WeightSliderGroup());
  }

  // Right side: Track analysis
  {
    contentLayout->addWidget(new TrackAnalysisInfo(),
                             2); // 2/3 of horizontal space
  }

  // Initialize HTTP server
  HttpWorker *server = new HttpWorker(&app);
  server->start();
  QObject::connect(&app, &QApplication::aboutToQuit, server,
                   &HttpWorker::destroy);

  window.show();
  return app.exec();
}

std::string formatDuration(float duration) {
  int totalSeconds = static_cast<int>(duration); // Extract total seconds
  int minutes = totalSeconds / 60;               // Calculate minutes
  int seconds = totalSeconds % 60;               // Calculate remaining seconds

  return fmt::format("{:02d}:{:02d}", minutes, seconds);
}