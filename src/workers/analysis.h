#ifndef ANALYSIS_H
#define ANALYSIS_H
#include "algorithmfactory.h"
#include <filesystem>
#include <qobject.h>
#include <sqlite3.h>
class MusicAnalyzer {
public:
  MusicAnalyzer();
  ~MusicAnalyzer();
  void analyze_directory(QString dir);
  // void analyze_file(QString file);
  void analyze_file(const std::filesystem::directory_entry file);

private:
  essentia::standard::AlgorithmFactory *factory;
  essentia::standard::Algorithm *mono_loader;
  essentia::standard::Algorithm *moving_average;
  essentia::standard::Algorithm *band_pass;
  essentia::standard::Algorithm *equalizer;
  essentia::standard::Algorithm *dc_remover;
  essentia::standard::Algorithm *onset_rate;
  essentia::standard::Algorithm *mfcc;

  sqlite3 *database{nullptr};
};
#endif