
#include "no_cover_art.h"
#include "webkit_widget.h"
#include "workers/db.h"
#include <cstdlib>
#include <qboxlayout.h>
#include <qnamespace.h>
#include <qobject.h>
#include <qpushbutton.h>
#include <qstringview.h>
#define WINDOW_MIN_W 1700
#define WINDOW_MIN_H 900

#include "conan_util.h"
#include <assets/favicon.h>
#include <assets/stylesheet.h>
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
#include <QSettings>
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
  setenv("LIBGL_ALWAYS_SOFTWARE", "0", 1);
  setenv("GALLIUM_DRIVER", "llvmpipe", 1);
  setenv("QT_QPA_PLATFORM", "xcb", 1);

  qputenv("QT_FATAL_WARNINGS", "1");
#ifdef NDEBUG
  spdlog::set_level(spdlog::level::info);
#else
  spdlog::set_level(spdlog::level::debug);
#endif

  QApplication app(argc, argv);
  app.setOrganizationName("cansudev");
  app.setOrganizationDomain("cansu.dev");
  app.setApplicationName("Conan");
  QSettings settings;

  // Application setup
  auto favicon = hex_to_icon(Resources::Favicon::decompress().data(),
                             Resources::Favicon::original_size, "ICO");
  app.setWindowIcon(favicon);
  app.setStyleSheet(QString::fromLocal8Bit(Resources::Stylesheet::decompress()));

  // Main window setup
  QMainWindow window;
  window.setWindowTitle("Conan");
  // no resize for you
  window.setMinimumSize(WINDOW_MIN_W, WINDOW_MIN_H);
  window.setMaximumSize(WINDOW_MIN_W, WINDOW_MIN_H);
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

  WebKitWidget *TrackPlayerInfo = new WebKitWidget(nullptr, nullptr);
  HttpWorker *server = new HttpWorker(&app);

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
        QObject::connect(server, &HttpWorker::cover_art_changed, coverArt,
                         &CoverArtLabel::on_cover_art_change,
                         Qt::QueuedConnection);
        PlaybackControlsLayout *playbackControls = new PlaybackControlsLayout();
        QObject::connect(playbackControls->play_pause, &QPushButton::clicked,
                         server, &HttpWorker::handle_play_pause_button_event,
                         Qt::QueuedConnection);
        QObject::connect(playbackControls->skip, &QPushButton::clicked, server,
                         &HttpWorker::handle_skip_button_event,
                         Qt::QueuedConnection);
        QObject::connect(playbackControls->stop, &QPushButton::clicked, server,
                         &HttpWorker::handle_stop_button_event,
                         Qt::QueuedConnection);
        playerLeft->addWidget(coverArt);
        playerLeft->addLayout(playbackControls);
      }

      // Right side of player (Track info)
      {
        QVBoxLayout *playerRight = new QVBoxLayout();
        playerSection->addLayout(playerRight);
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
  server->start();
  TrackPlayerInfo->loadURL("http://localhost:31311");
  QObject::connect(&app, &QApplication::aboutToQuit, server,
                   &HttpWorker::deleteLater);

  window.show();
  return app.exec();
}

std::string formatDuration(float duration) {
  int totalSeconds = static_cast<int>(duration); // Extract total seconds
  int minutes = totalSeconds / 60;               // Calculate minutes
  int seconds = totalSeconds % 60;               // Calculate remaining seconds

  return fmt::format("{:02d}:{:02d}", minutes, seconds);
}

// ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣤⣾⡿⠿⢿⣦⡀⠀⠀⠀⠀⠀⠀
// ⠀⠀⢀⣶⣿⣶⣶⣶⣦⣤⣄⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣰⣿⠟⠁⣀⣤⡄⢹⣷⡀⠀⠀⠀⠀⠀
// ⠀⠀⢸⣿⡧⠤⠤⣌⣉⣩⣿⡿⠶⠶⠒⠛⠛⠻⠿⠶⣾⣿⣣⠔⠉⠀⠀⠙⡆⢻⣷⠀⠀⠀⠀⠀
// ⠀⠀⢸⣿⠀⠀⢠⣾⠟⠋⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣾⣿⡃⠀⠀⠀⠀⠀⢻⠘⣿⡀⠀⠀⠀⠀
// ⠀⠀⠘⣿⡀⣴⠟⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠉⠛⠻⢶⣤⣀⠀⢘⠀⣿⡇⠀⠀⠀⠀
// ⠀⠀⠀⢿⣿⠋⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⠉⠛⢿⣴⣿⠀⠀⠀⠀⠀
// ⠀⠀⠀⣸⡟⠀⠀⠀⣴⡆⠀⠀⠀⠀⠀⠀⠀⣷⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠻⣷⡀⠀⠀⠀⠀
// ⠀⠀⢰⣿⠁⠀⠀⣰⠿⣇⠀⠀⠀⠀⠀⠀⠀⢻⣷⡀⠀⢠⡄⠀⠀⠀⠀⠀⡀⠀⠹⣷⠀⠀⠀⠀
// ⠀⠀⣾⡏⠀⢀⣴⣿⣤⢿⡄⠀⠀⠀⠀⠀⠀⠸⣿⣷⡀⠘⣧⠀⠀⠀⠀⠀⣷⣄⠀⢻⣇⠀⠀⠀
// ⠀⠀⢻⣇⠀⢸⡇⠀⠀⠀⢻⣄⠀⠀⠀⠀⠀⣤⡯⠈⢻⣄⢻⡄⠀⠀⠀⠀⣿⡿⣷⡌⣿⡄⠀⠀
// ⠀⢀⣸⣿⠀⢸⡷⣶⣶⡄⠀⠙⠛⠛⠛⠛⠛⠃⣠⣶⣄⠙⠿⣧⠀⠀⠀⢠⣿⢹⣻⡇⠸⣿⡄⠀
// ⢰⣿⢟⣿⡴⠞⠀⠘⢿⡿⠀⠀⠀⠀⠀⠀⠀⠀⠈⠻⣿⡇⠀⣿⡀⢀⣴⠿⣿⣦⣿⠃⠀⢹⣷⠀
// ⠀⢿⣿⠁⠀⠀⠀⠀⠀⠀⠀⢠⣀⣀⡀⠀⡀⠀⠀⠀⠀⠀⠀⣿⠛⠛⠁⠀⣿⡟⠁⠀⠀⢀⣿⠂
// ⠀⢠⣿⢷⣤⣀⠀⠀⠀⠀⠀⠀⠉⠉⠉⠛⠉⠀⠀⠀⠀⠀⢠⡿⢰⡟⠻⠞⠛⣧⣠⣦⣀⣾⠏⠀
// ⠀⢸⣿⠀⠈⢹⡿⠛⢶⡶⢶⣤⣤⣤⣤⣤⣤⣤⣤⣶⠶⣿⠛⠷⢾⣧⣠⡿⢿⡟⠋⠛⠋⠁⠀⠀
// ⠀⣾⣧⣤⣶⣟⠁⠀⢸⣇⣸⠹⣧⣠⡾⠛⢷⣤⡾⣿⢰⡟⠀⠀⠀⣿⠋⠁⢈⣿⣄⠀⠀⠀⠀⠀
// ⠀⠀⠀⣼⡏⠻⢿⣶⣤⣿⣿⠀⠈⢉⣿⠀⢸⣏⠀⣿⠈⣷⣤⣤⣶⡿⠶⠾⠋⣉⣿⣦⣀⠀⠀⠀
// ⠀⠀⣼⡿⣇⠀⠀⠙⠻⢿⣿⠀⠀⢸⣇⠀⠀⣻⠀⣿⠀⣿⠟⠋⠁⠀⠀⢀⡾⠋⠉⠙⣿⡆⠀⠀
// ⠀⠀⢻⣧⠙⢷⣤⣦⠀⢸⣿⡄⠀⠀⠉⠳⣾⠏⠀⢹⣾⡇⠀⠀⠙⠛⠶⣾⡁⠀⠀⠀⣼⡇⠀⠀
// ⠀⠀⠀⠙⠛⠛⣻⡟⠀⣼⣿⣇⣀⣀⣀⡀⠀⠀⠀⣸⣿⣇⠀⠀⠀⠀⠀⠈⢛⣷⠶⠿⠋⠀⠀⠀
// ⠀⠀⠀⠀⠀⢠⣿⣅⣠⣿⠛⠋⠉⠉⠛⠻⠛⠛⠛⠛⠋⠻⣧⡀⣀⣠⢴⠾⠉⣿⣆⠀⠀⠀⠀⠀
// ⠀⠀⠀⠀⠀⣼⡏⠉⣿⡟⠁⠀⠀⠀⢀⠀⠀⠀⠀⠀⠀⠀⠙⠿⣿⣌⠁⠀⠀⠈⣿⡆⠀⠀⠀⠀
// ⠀⠀⠀⠀⠀⣿⣇⣠⣿⣿⡶⠶⠶⣶⣿⣷⡶⣶⣶⣶⣶⡶⠶⠶⢿⣿⡗⣀⣤⣾⠟⠁⠀⠀⠀⠀
// ⠀⠀⠀⠀⠀⠈⠙⠛⢻⣿⡇⠀⠀⣿⡟⠛⠷⠶⠾⢿⣧⠁⠀⠀⣸⡿⠿⠟⠉⠀⠀⠀⠀⠀⠀⠀
// ⠀⠀⠀⠀⠀⠀⠀⠀⠀⢿⣷⣦⣤⡿⠀⠀⠀⠀⠀⠀⢿⣧⣤⣼⣿⠇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀