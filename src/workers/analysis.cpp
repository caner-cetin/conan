#include "analysis.h"
#include "conan_util.h"
#include "essentia.h"
#include "nlohmann/json.hpp"
#include "parameter.h"
#include "types.h"
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iterator>
#include <models/discogs_effnet_bs64.h>
#include <qobject.h>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <string>
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <unistd.h>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif
using namespace nlohmann;
using namespace essentia;
using namespace essentia::standard;
MusicAnalyzer::MusicAnalyzer(QObject *parent) : QObject(parent) {
  db = new DB(parent);
  db->create_all_tables_if_not_exists();

  essentia::init();

  factory = &factory->instance();
  audio_loader = factory->create("MonoLoader");
  dc_remover = factory->create("DCRemoval");
  equalizer = factory->create("EqualLoudness");
  band_pass = factory->create("BandPass", "cutoffFrequency", 2000, "bandwidth",
                              19980, "sampleRate", 44100);
  moving_average = factory->create("MovingAverage");
  spectral_centroid = factory->create("SpectralCentroidTime");
  distribution_shape = factory->create("DistributionShape");
  spectrum = factory->create("Spectrum");
  spectral_flux = factory->create("Flux");
  spectral_complexity = factory->create("SpectralComplexity");
  spectral_peaks = factory->create("SpectralPeaks");
  entropy = factory->create("Entropy");
  zcr = factory->create("ZeroCrossingRate");
  mel_bands = factory->create("MelBands");
  pitch_salience = factory->create("PitchSalience");
  chord_descriptors = factory->create("ChordsDescriptors");
  chord_detection = factory->create("ChordsDetection");
  key_extractor = factory->create("KeyExtractor");
  hpcp_crest = factory->create("HPCP");
  dynamic_complexity = factory->create("DynamicComplexity");
  rhythm_extractor = factory->create("RhythmExtractor2013");
  onset_rate = factory->create("OnsetRate");
  danceability = factory->create("Danceability");
  centralMoments = factory->create("CentralMoments");
  frameCutter =
      factory->create("FrameCutter", "frameSize", 2048, "hopSize", 1024);
  window = factory->create("Windowing", "type", "blackmanharris62");

  // TensorflowPredictEffnetDiscogs takes only filenames as model files.
  // i cant feed byte data into it directly.
  //
  // todo: this is different on windows
  // http://msdn.microsoft.com/en-us/library/aa363875%28VS.85%29.aspx
  char discogs_effnet_b64_filename_tmpl[] = "/tmp/conan_models.XXXXXX";
  int fd = mkstemp(discogs_effnet_b64_filename_tmpl);

  if (fd == -1) {
    throw std::runtime_error("cannot open temporary file");
  }
  write(fd, Resources::DiscogsEffnetBS64::decompress().data(),
        Resources::DiscogsEffnetBS64::original_size);
  close(fd);
  discogs_effnet_b64_filename = discogs_effnet_b64_filename_tmpl;
  spdlog::debug("discogs effnet b64 is saved to: {}",
                discogs_effnet_b64_filename);
}

MusicAnalyzer::~MusicAnalyzer() {
  delete audio_loader;
  delete dc_remover;
  delete equalizer;
  delete band_pass;
  delete moving_average;
  unlink(discogs_effnet_b64_filename);

  essentia::shutdown();
}

void MusicAnalyzer::analyze_directory(QString directory) {
  auto files = find_audio_files(directory.toStdString());
  for (std::string file : files) {
    analyze_file(file.c_str());
  }
  reset_all_algorithms();
}

/**
 * @brief
 *
 * @param fp  absolute path of file
 */
void MusicAnalyzer::analyze_file(const char *fp) {
  TagLib::FileRef metadata_ref(fp);
  if (metadata_ref.tag()) {
    spdlog::info("analyzing {} - {}", metadata_ref.tag()->artist().toCString(),
                 metadata_ref.tag()->title().toCString());
  } else {
    spdlog::info("analyzing {}", fp);
  }
  ParameterMap params;
  params.add("filename", fp);
  params.add("resampleQuality", 4);
  params.add("sampleRate", 44100);
  audio_loader->configure(params);
  params.clear();

  std::vector<Real> audio;
  audio_loader->reset();
  dc_remover->reset();
  equalizer->reset();
  band_pass->reset();
  moving_average->reset();

  audio_loader->output("audio").set(audio);
  dc_remover->input("signal").set(audio);
  dc_remover->output("signal").set(audio);
  equalizer->input("signal").set(audio);
  equalizer->output("signal").set(audio);
  band_pass->input("signal").set(audio);
  band_pass->output("signal").set(audio);
  moving_average->input("signal").set(audio);
  moving_average->output("signal").set(audio);

  audio_loader->compute();
  dc_remover->compute();
  equalizer->compute();
  band_pass->compute();
  moving_average->compute();
  TrackFeatures features;

  std::vector<float> pos_audio;
  std::copy_if(audio.begin(), audio.end(), std::back_inserter(pos_audio),
               [](float sig) { return sig >= 0; });
  entropy->reset();
  entropy->input("array").set(pos_audio);
  entropy->output("entropy").set(features.entropy);
  entropy->compute();

  zcr->reset();
  zcr->input("signal").set(audio);
  zcr->output("zeroCrossingRate").set(features.zeroCrossingRate);
  zcr->compute();

  std::string key;
  std::string scale;
  float key_strength;
  key_extractor->reset();
  key_extractor->input("audio").set(audio);
  key_extractor->output("key").set(key);
  key_extractor->output("scale").set(scale);
  key_extractor->output("strength").set(key_strength);
  key_extractor->compute();

  dynamic_complexity->reset();
  dynamic_complexity->input("signal").set(audio);
  dynamic_complexity->output("dynamicComplexity")
      .set(features.dynamicComplexity);
  dynamic_complexity->output("loudness").set(features.loudness);
  dynamic_complexity->compute();

  std::vector<float> bpm_ticks;
  float confidence;
  std::vector<float> bpm_estimates;
  std::vector<float> bpm_intervals;
  rhythm_extractor->reset();
  rhythm_extractor->input("signal").set(audio);
  rhythm_extractor->output("bpm").set(features.bpm);
  rhythm_extractor->output("ticks").set(bpm_ticks);
  rhythm_extractor->output("confidence").set(confidence);
  rhythm_extractor->output("estimates").set(bpm_estimates);
  rhythm_extractor->output("bpmIntervals").set(bpm_intervals);
  rhythm_extractor->compute();
  features.beats_count = bpm_ticks.size();

  std::vector<float> onsets;
  onset_rate->reset();
  onset_rate->input("signal").set(audio);
  onset_rate->output("onsetRate").set(features.onset_rate);
  onset_rate->output("onsets").set(onsets);
  onset_rate->compute();

  std::vector<Real> frame;
  std::vector<Real> windowedFrame;
  std::vector<std::vector<float>> pcp;
  std::vector<float> pitch_salience_all;
  std::vector<std::vector<float>> mel_bands_all;
  std::vector<float> flux_all;
  std::vector<float> spectral_complexity_all;
  std::vector<float> spectral_centroid_all;
  std::vector<float> spectral_spread_all;
  std::vector<float> spectral_skewness_all;
  std::vector<float> spectral_kurtosis_all;

  frameCutter->input("signal").set(audio);
  frameCutter->output("frame").set(frame);
  while (true) {
    frameCutter->compute();

    if (!frame.size())
      break;

    window->input("frame").set(frame);
    window->output("frame").set(windowedFrame);
    window->compute();

    std::vector<float> spectrum_output;
    spectrum->reset();
    spectrum->input("frame").set(windowedFrame);
    spectrum->output("spectrum").set(spectrum_output);
    spectrum->compute();

    float flux;
    spectral_flux->reset();
    spectral_flux->input("spectrum").set(spectrum_output);
    spectral_flux->output("flux").set(flux);
    spectral_flux->compute();
    flux_all.push_back(flux);

    float spectral_complexity_output;
    spectral_complexity->reset();
    spectral_complexity->input("spectrum").set(spectrum_output);
    spectral_complexity->output("spectralComplexity")
        .set(spectral_complexity_output);
    spectral_complexity->compute();
    spectral_complexity_all.push_back(spectral_complexity_output);

    std::vector<float> mel_bands_output;
    mel_bands->reset();
    mel_bands->input("spectrum").set(spectrum_output);
    mel_bands->output("bands").set(mel_bands_output);
    try {
      mel_bands->compute();
      mel_bands_all.push_back(mel_bands_output);
    } catch (essentia::EssentiaException) {
      //   what():  TriangularBands: the 'frequencyBands' parameter contains a
      //   value above the Nyquist frequency (22050 Hz): 22050
      features.melbands = {};
    }
    float pitch_salience_output;
    pitch_salience->reset();
    pitch_salience->input("spectrum").set(spectrum_output);
    pitch_salience->output("pitchSalience").set(pitch_salience_output);
    pitch_salience->compute();
    pitch_salience_all.push_back(pitch_salience_output);

    std::vector<float> frequencies;
    std::vector<float> magnitudes;
    spectral_peaks->reset();
    spectral_peaks->input("spectrum").set(spectrum_output);
    spectral_peaks->output("frequencies").set(frequencies);
    spectral_peaks->output("magnitudes").set(magnitudes);
    spectral_peaks->compute();

    std::vector<float> hpcp;
    hpcp_crest->reset();
    hpcp_crest->input("frequencies").set(frequencies);
    hpcp_crest->input("magnitudes").set(magnitudes);
    hpcp_crest->output("hpcp").set(hpcp);
    hpcp_crest->compute();
    pcp.push_back(hpcp);

    float spectral_centroid_output;
    spectral_centroid->reset();
    spectral_centroid->input("array").set(spectrum_output);
    spectral_centroid->output("centroid").set(spectral_centroid_output);
    spectral_centroid->compute();
    spectral_centroid_all.push_back(spectral_centroid_output);

    std::vector<float> cMoments;
    centralMoments->reset();
    centralMoments->input("array").set(spectrum_output);
    centralMoments->output("centralMoments").set(cMoments);
    centralMoments->compute();

    float spectral_spread;
    float spectral_skewness;
    float spectral_kurtosis;
    distribution_shape->reset();
    distribution_shape->input("centralMoments").set(cMoments);
    distribution_shape->output("spread").set(spectral_spread);
    distribution_shape->output("skewness").set(spectral_skewness);
    distribution_shape->output("kurtosis").set(spectral_kurtosis);
    distribution_shape->compute();

    spectral_spread_all.push_back(spectral_spread);
    spectral_skewness_all.push_back(spectral_skewness);
    spectral_kurtosis_all.push_back(spectral_kurtosis);
  }

  features.hpcp = pcp;
  features.pitchSalience = pitch_salience_all;
  features.melbands = mel_bands_all;
  features.spectral_complexity = spectral_complexity_all;
  features.spectral_centroid = spectral_centroid_all;
  features.spectral_spread = spectral_spread_all;
  features.spectral_skewness = spectral_skewness_all;
  features.spectral_kurtosis = spectral_kurtosis_all;
  features.flux = flux_all;

  std::vector<std::string> chords;
  std::vector<float> chord_strength;
  chord_detection->reset();
  chord_detection->input("pcp").set(pcp);
  chord_detection->output("chords").set(chords);
  chord_detection->output("strength").set(chord_strength);
  chord_detection->compute();

  std::string chords_key;
  std::string chords_scale;
  std::vector<float> chords_histogram;
  chord_descriptors->reset();
  chord_descriptors->input("key").set(key);
  chord_descriptors->input("scale").set(scale);
  chord_descriptors->input("chords").set(chords);
  chord_descriptors->output("chordsNumberRate").set(features.chordsNumberRate);
  chord_descriptors->output("chordsChangesRate")
      .set(features.chordsChangesRate);
  chord_descriptors->output("chordsHistogram").set(chords_histogram);
  chord_descriptors->output("chordsKey").set(chords_key);
  chord_descriptors->output("chordsScale").set(chords_scale);
  chord_descriptors->compute();

  features.danceability = 0;
  json j;
  features.to_json(j, features);
  spdlog::debug("{}", j.dump());
}

void MusicAnalyzer::reset_all_algorithms() {
  audio_loader->reset();
  dc_remover->reset();
  equalizer->reset();
  band_pass->reset();
  moving_average->reset();
  spectrum->reset();
  spectral_centroid->reset();
  centralMoments->reset();
  distribution_shape->reset();
  spectral_flux->reset();
  spectral_complexity->reset();
  entropy->reset();
  zcr->reset();
  mel_bands->reset();
  pitch_salience->reset();
  spectral_peaks->reset();
  hpcp_crest->reset();
  key_extractor->reset();
  chord_detection->reset();
  chord_descriptors->reset();
  dynamic_complexity->reset();
  onset_rate->reset();
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