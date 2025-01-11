#include "analysis.h"
#include "essentia.h"
#include "parameter.h"
#include "types.h"
#include <filesystem>
#include <iostream>
#include <qobject.h>
#include <vector>
using namespace essentia;
using namespace essentia::standard;
MusicAnalyzer::MusicAnalyzer() {
  essentia::init();

  factory = &AlgorithmFactory::instance();
  mono_loader = factory->create("MonoLoader");
  dc_remover = factory->create("DCRemoval");
  equalizer = factory->create("EqualLoudness");
  band_pass = factory->create("BandPass", "cutoffFrequency", 2000, "bandwidth",
                              19980, "sampleRate", 44100);
  moving_average = factory->create("MovingAverage");
}

void MusicAnalyzer::analyze_directory(QString directory) {
  for (const auto &dir_entry :
       std::filesystem::recursive_directory_iterator(directory.toStdString())) {
    analyze_file(dir_entry);
  }
}

void MusicAnalyzer::analyze_file(const std::filesystem::directory_entry file) {
  ParameterMap params;
  params.add("filename", file.path().string());
  params.add("resampleQuality", 4);
  params.add("sampleRate", 16000);
  mono_loader->configure(params);
  params.clear();

  std::vector<Real> audio;
  mono_loader->output("audio").set(audio);
  dc_remover->input("signal").set(audio);
  dc_remover->output("signal").set(audio);
  equalizer->input("signal").set(audio);
  equalizer->output("signal").set(audio);
  band_pass->input("signal").set(audio);
  band_pass->output("signal").set(audio);
  moving_average->input("signal").set(audio);
  moving_average->output("signal").set(audio);

  std::cout << audio << std::endl;
}

MusicAnalyzer::~MusicAnalyzer() {
  delete mono_loader;
  delete dc_remover;
  delete equalizer;
  delete band_pass;
  delete moving_average;
  delete factory;

  essentia::shutdown();
}

// ⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡿⢟⣡⣶⣿⣿⣿⣿⣶⣮⣙⠻⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠟⣩⣽⣶⣶⢰⡆⣮⣙⠿⣿⣿⣿⣿⣿⣿⣿⣿
// ⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡟⣱⣿⣷⣬⡛⢿⣿⣿⣿⣿⣿⣧⡘⡿⢛⣛⣽⣽⣷⣶⣶⣮⣭⣍⡛⠛⡋⢉⠙⠿⠿⠿⠿⠿⠿⢣⣾⣿⣿⢿⣿⡇⡇⢻⣿⣷⠘⢿⣿⣿⣿⣹⣿⣿
// ⣿⣿⣿⣿⣿⣿⣿⣿⣿⡿⠰⣛⣻⠿⠿⣿⣶⣍⢻⣿⣿⣿⣿⣷⡸⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣷⣤⣩⣶⣾⣿⣿⣿⣟⣒⣒⣉⣙⠾⣿⣿⡇⡇⢸⣿⣿⢰⡸⣿⠹⡚⣿⣿⣿
// ⣿⣿⣿⣿⣿⣿⣿⣿⣿⡇⣯⣽⣿⣿⣷⣦⣝⢻⣷⡙⣮⢍⢿⣿⡇⣒⡒⠿⣷⣶⣭⡛⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣵⣄⡙⢱⡇⣿⣿⡟⣼⡇⢻⡀⠉⠮⣭⣿
// ⣿⣿⣿⣿⣿⣿⣿⣿⣿⠃⣉⣭⣭⣍⣛⠿⣿⣷⡜⢿⣌⢎⣆⢻⢣⣿⣿⣷⣶⣍⠻⣿⣿⣿⣿⣿⣿⡿⠿⣿⡿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣦⠰⣿⡟⣰⡿⢳⠸⠘⡶⣁⣿⣿
// ⣿⣿⣿⣿⣿⣿⣿⣿⣿⣶⡘⣿⣿⣿⣿⣷⣌⠻⣿⡌⣿⡌⣿⢌⣾⣿⣿⣿⣿⣿⣷⣬⣿⣿⣿⣿⣿⣿⣷⣮⡑⠌⢿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣧⠈⠴⠿⠡⢾⣶⣶⣶⣿⣿⣿
// ⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣷⣌⠻⣿⣿⣿⣿⣧⢹⣿⠸⠟⣠⣾⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣷⡌⠊⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣶⣿⣿⡿⠶⠌⠙⢿⣿⣿⣿
// ⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡗⣠⣍⡙⠛⠛⠃⠉⣴⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡿⣿⡆⣻⣭⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣷⣤⣌⠻⣷⣶⣿⣿⣿
// ⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⢣⣿⣿⢡⣿⡇⣾⠘⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡷⢼⣧⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣟⠻⣿⣷⡌⢻⣿⣿⣿
// ⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡿⢸⣿⡿⣼⡿⢇⣿⣇⢿⣿⣿⣿⣿⣿⢿⣿⣿⣿⣿⢿⣿⣿⣿⣿⣿⠟⡡⢄⡀⠈⠋⠩⢍⠻⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣷⣦⠙⢿⡄⢿⣿⣿
// ⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡇⣾⣿⡇⣿⣧⣿⣿⣿⣼⣿⣿⣿⣿⡏⣿⣿⣿⠟⡏⣼⣿⣿⡟⢹⣿⢸⣿⣦⣥⣤⣴⣭⣤⣤⢹⣿⢻⣿⣿⣿⣿⣿⡿⣿⣿⣿⣿⣧⠀⠻⢸⣿⣿
// ⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⢳⣿⣿⡇⣿⣿⣿⣿⡇⣿⣿⣿⣿⢻⡇⣿⣿⠾⠀⠃⠛⣃⣛⠇⠘⠿⣎⠻⣿⣿⣿⣿⣿⣿⡟⣸⡹⢸⢿⣿⣿⣿⣿⣧⠹⣿⣆⠹⣿⡄⠆⣿⣿⣿
// ⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡏⣸⣿⣿⠇⢻⡿⣿⣯⠱⠹⣿⣧⠸⡎⠃⠈⠡⢶⣦⠀⢀⢙⠻⠟⠦⢑⣶⣶⣿⣿⣿⣿⣉⠉⠄⢉⡁⠉⠀⣿⣿⣿⣿⣿⡇⠘⣿⡆⠹⢁⣿⣿⣿⣿
// ⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⢁⣿⣿⠏⣾⠸⣧⢹⣿⣷⡑⣌⢛⣓⣀⣤⣴⢲⡾⠿⠿⠿⢿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡿⠟⣃⣾⣿⡏⣹⣿⣿⢡⡄⡿⢳⣠⣾⣿⣿⣿⣿
// ⣿⣿⣿⣿⣿⣿⣿⣿⣿⡟⣼⣿⡏⣼⣿⡆⢿⣧⠙⠅⠲⢦⣼⡿⣷⣜⠻⠀⠀⠀⠀⠀⠈⠻⢿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⢿⣷⣮⣍⣛⠉⣰⣿⡿⠃⡞⠀⣡⣾⣿⣿⣿⣿⣿⣿
// ⣿⣿⣿⣿⣿⣿⣿⣿⣿⢁⣿⡟⣸⣿⣿⠟⠘⠏⣠⣼⣿⡀⢹⣿⣦⡍⢠⣴⣾⣿⣿⣿⣷⣦⣄⢹⣿⣿⣿⣿⣿⣿⡿⠛⠁⠈⠁⠀⠉⣀⡬⢉⣥⡔⠀⣤⣿⣿⣿⣿⣿⣿⣿⣿⣿
// ⣿⣿⣿⣿⣿⣿⣿⣿⡟⣼⡿⢱⣿⣿⠟⡄⣿⡄⢻⣏⠇⣄⢻⣿⣿⣷⣿⣿⣿⡿⣿⣟⢿⣿⣿⣾⣿⣿⣿⣿⠻⣟⣠⣶⣿⣿⣿⣦⡀⠘⠀⣾⣿⣿⡆⣭⣭⣽⣿⣿⣿⣿⣿⣿⣿
// ⣿⣿⣿⣿⣿⣿⣿⣿⠃⡿⣡⣟⣿⠏⣴⢠⣿⡷⡌⢻⣷⡌⠘⣿⣿⣿⣿⣿⣿⣧⣿⣿⣼⣿⣿⣿⣿⣿⣿⣿⠰⣿⣿⣿⣿⣿⡟⣿⣷⡇⠃⠹⣿⣿⡇⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿
// ⣿⣿⣿⣿⣿⣿⣿⡿⡸⢡⣿⣿⢃⣼⣿⢸⣿⡇⠻⣦⣙⠳⠖⢿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡟⣿⣿⣿⣯⣴⣿⣿⣿⣿⣿⣧⣼⣿⣧⢸⣦⢹⣿⡇⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿
// ⣿⣿⣿⣿⣿⣿⣿⠇⢡⣿⡿⢣⣾⣿⡏⣿⣿⣿⣿⣿⣿⣶⣦⡈⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡿⢃⡛⠿⠿⠟⣻⣿⣿⣿⣿⣿⣿⣿⣿⡟⣀⢻⡆⢻⡇⣿⣿⣿⣿⣿⣿⣿⠛⠋⠁
// ⣿⣿⣿⣿⣿⣿⡟⢠⣿⡿⣡⣿⣿⣿⠇⣿⣿⣿⢠⣤⣤⠤⣤⣤⡌⢿⣿⣿⣿⣿⣿⣿⣿⣿⣷⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡿⠁⣿⣆⢻⣆⠃⢰⣶⣶⣶⣶⠆⠀⠀⠀⠀
// ⢿⣿⣿⣿⣿⣿⢣⣿⡟⣰⣿⣿⣿⣿⢰⣿⣿⣿⣿⣿⣿⣼⣿⢋⠔⠠⠙⢿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡿⢋⣴⡇⣿⣿⢆⢻⣆⢻⣿⣿⣿⣿⣷⡀⠀⠀⠀
// ⢸⣿⣿⣿⣿⡇⣼⠏⣴⣿⣿⣿⣿⡟⢸⣿⣿⣿⣿⣿⣿⣿⣧⡞⢰⣿⣿⡆⣈⡛⠿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡿⠟⣩⢰⣿⣿⡇⡿⣱⣿⡎⢿⢸⣿⣿⣿⣿⣿⣷⠀⠀⠀
// ⢸⣿⣿⣿⡟⢸⠏⣼⣿⣿⣿⣿⣿⡇⢸⣿⣿⣿⣿⣿⣿⣿⣿⣷⣼⣿⣿⢰⣿⣿⣷⣦⣍⣛⠿⠿⠿⣿⠿⠿⢛⣉⣵⣶⢻⣿⣾⣿⣿⡇⣿⣿⣿⣿⡌⠀⣿⣿⣿⣿⣿⣿⣧⠀⠀
// ⢸⣿⣿⣿⢡⠏⣼⣿⣿⣿⣿⣿⣿⢰⢸⣿⣿⣿⣿⣿⣿⣿⣿⣿⠿⠿⠏⣿⣿⣿⣿⣿⣿⣿⣿⣶⣤⣀⡀⣿⣿⣿⣿⣿⣼⣿⣿⣿⣿⣿⢿⣿⣿⣿⣿⡆⣿⣿⣿⣿⣿⣿⣿⣿⣦
// ⣾⣿⣿⠏⠎⣼⣿⣿⣿⣿⣿⣿⡏⠚⢘⣛⣉⣭⣥⣭⣥⣶⣶⣶⣿⣌⠀⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡇⢿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⢸⣿⣿⣿⣿⣿⢸⣿⣿⣿⣿⣿⣿⣿⣿
// ⣿⣿⡿⠐⣼⣿⣿⣿⣿⣿⠟⣩⣴⣶⣦⡙⢿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣷⣮⡙⢿⣿⣿⣿⣿⣿⠟⡩⠩⠤⠄⠭⢭⣽⣿⣟⡻⠿⣿⣿⣿⢸⣿⣿⣿⣿⣿⡄⣿⣿⣿⣿⣿⣿⣿⣿
// ⠻⠿⠃⣰⣿⣿⣿⣿⡿⠁⣴⣿⣿⣿⣿⣿⣆⠻⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣦⣌⠛⣛⡛⠇⣷⣬⣃⠛⠛⢛⣿⣿⣿⣿⠳⠄⡐⠶⢶⡶⢌⠙⠻⣿⣿⣷⢻⣿⣿⣿⣿⣿⣿⣿
// ⣿⡇⢠⣿⣿⣿⣿⡿⢁⣴⣿⣿⣿⣿⣿⣿⣿⣷⣌⠻⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣷⡌⢿⡇⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣷⣄⠙⠻⠷⠶⠒⠀⠀⠹⣿⣿⡈⣿⣿⣿⣿⣿⢣⣿
// ⡿⢡⣿⣿⣿⣿⣿⠃⣿⣿⣿⣿⣿⣿⣿⡿⣿⣿⣿⣷⣘⠻⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣦⡄⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣶⣤⣴⣶⡆⢺⡂⣿⣿⣧⢸⣿⣿⣿⣿⣸⣿
// ⣇⣾⣿⣿⣿⣿⡟⢸⣿⣿⣿⣿⣿⣿⣿⣿⣮⡻⣿⣿⣿⣷⣌⠻⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡇⢿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠁⣼⡇⣿⣿⣿⡎⢿⣿⣿⣿⣿⣿
// ⣿⣿⣿⣿⣿⣿⡇⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣮⡻⣿⣿⣿⣷⣬⡙⠿⣿⣿⣿⣿⣿⣿⣿⢻⣶⣶⣬⣭⣭⣭⣽⣿⣿⣿⣛⣛⣛⣛⣛⣛⣋⠀⢻⡇⢻⣿⣿⣷⡸⡿⠿⠿⢹⣿
// ⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣦⡙⣿⣿⣿⣿⣷⣦⣉⠛⠿⢿⣿⣿⣺⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠟⣵⢠⡘⣷⡈⣿⣿⣿⣇⠁⡟⣩⣁⠹