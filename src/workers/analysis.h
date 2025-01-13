#ifndef ANALYSIS_H
#define ANALYSIS_H
#include "algorithmfactory.h"
#include "nlohmann/json.hpp"
#include "workers/db.h"
#include <qobject.h>
#include <qtmetamacros.h>
#include <sqlite3.h>
#include <vector>

using namespace essentia;
using namespace essentia::standard;
class MusicAnalyzer : QObject {
  Q_OBJECT;

public:
  MusicAnalyzer(QObject *parent = nullptr);
  ~MusicAnalyzer();
  void analyze_directory(QString dir);
  void analyze_file(const char *file);

private:
  void reset_all_algorithms();
  DB *db;
  AlgorithmFactory *factory;
  Algorithm *audio_loader;
  Algorithm *moving_average;
  //  by stripping noise outside of the frequencies capable of human ear, we
  //  assure that algorithms and models actually run on the data that is
  //  relevant to the human ear, such as Low bass frequencies (important for
  //  genre detection) High frequencies (important for detecting cymbals,
  //  hi-hats, etc.) Mid-range frequencies (important for detecting vocals,
  //  guitars, etc.)
  Algorithm *band_pass;
  // Think about how your ears work. When you listen to music, your ears don't
  // hear all frequencies equally well - they're naturally better at hearing
  // some frequencies than others. For example, you hear mid-range frequencies
  // (like people talking) much better than very low or very high frequencies.
  // The equal loudness filter tries to mimic how human ears work. It boosts or
  // reduces different frequencies to match how we actually hear them. It's like
  // having a special pair of headphones that automatically adjusts different
  // sounds to match how your ears naturally process them. Example:
  // - Without the filter: A very low bass note and a mid-range piano note might
  // have the same volume in the audio file
  // - With the filter: The bass note gets boosted because our ears naturally
  // hear it as quieter than it actually is
  Algorithm *equalizer;
  //  DC offset is basically a constant shift in the audio signal that's at 0 Hz
  //  (meaning it doesn't change/vibrate at all), and human ears can only hear
  //  sounds between roughly 20 Hz and 20,000 Hz. But DC offset can cause two
  //  big problems:
  //  - It wastes headroom in your audio signal. Think of it like trying to fit
  //  a tall person in a room with a low ceiling - if they're standing on a
  //  platform (DC offset),
  //    they might hit their head. Similarly, if your audio wave is shifted up
  //    or down, you have less room for the actual audio signal before it
  //    distorts.
  //  - It can mess up your audio analysis algorithms. Many audio analysis
  //    tools expect the audio to be centered around zero.
  //    If there's DC offset, it can throw off measurements of things like
  //    amplitude, energy, and other features you might want to analyze.
  Algorithm *dc_remover;
  // Spectral Centroid: The "brightness" or "sharpness" of the sound
  // Example: Comparing Darkthrone's "Transilvanian Hunger" vs. Mayhem's
  // "Freezing Moon"
  // Darkthrone has a more trebly, harsh sound (higher centroid)
  // while Mayhem's recording has more mid-range focus (lower centroid)
  // Higher similarity = songs have similar levels of brightness/harshness
  Algorithm *spectral_centroid;
  // There is three output from DistributionShape:
  //
  // Spectral Kurtosis: How "peaky" or "flat" the frequency distribution is
  // Example: Comparing Necrophagist's "Epitaph" vs. Gorguts' "Obscura"
  // Necrophagist has very precise, sharp peaks in frequency (high kurtosis)
  // due to their technical, clean production
  // Gorguts has a more chaotic, spread-out frequency distribution (lower
  // kurtosis)
  // Higher similarity = songs have similar levels of frequency "peakiness"
  //
  // Spectral Skewness: Whether frequencies are concentrated in lower or higher
  // ranges
  // Example: Comparing Darkthrone's "Transilvanian Hunger" vs.
  // Deathspell Omega's "Kenose"
  // Darkthrone's raw production style pushes heavily into high frequencies
  // (positive skew) Deathspell Omega has more balanced frequency distribution
  // (neutral skew)
  // Higher similarity = songs have similar frequency balance/distribution
  //
  // Spectral Spread: How widely the frequencies are distributed
  // Example: Comparing Portal's "Vexovoid" vs. Morbid Angel's "Blessed Are The
  // Sick"
  // Portal has extremely wide frequency spread due to their chaotic,
  // noisy style Morbid Angel has more focused frequency ranges due to clearer
  // production
  // Higher similarity = songs have similar width of frequency distribution
  Algorithm *distribution_shape;
  Algorithm *centralMoments;
  // Spectral Flux: How quickly the spectrum changes over time
  // Example: Comparing Converge's "Jane Doe" vs. Blut Aus Nord's "777 -
  // Sect(s)" Converge has rapid, aggressive changes in frequency content (high
  // flux) due to their chaotic hardcore-influenced style Blut Aus Nord has more
  // gradual, atmospheric changes (lower flux)
  // Higher similarity = songs have similar rates of spectral change
  Algorithm *spectral_flux;
  Algorithm *spectrum;
  // Spectral Complexity: Measure of the complexity/richness of the spectrum
  // Example: Comparing Ulcerate's "Stare Into Death and Be Still" vs.
  // Immortal's "Pure Holocaust"
  // Ulcerate has very complex, layered frequency content (high complexity) due
  // to their dense, technical approach Immortal has simpler, more
  // straightforward spectral content (lower complexity)
  // Higher similarity = songs have similar levels of spectral richness
  Algorithm *spectral_complexity;
  // Entropy: How "random" or "predictable" the frequency content is
  // Example: Comparing Deathspell Omega's "Fas - Ite, Maledicti" vs.
  // Dissection's "Storm of the Light's Bane" Deathspell Omega has more
  // unpredictable, chaotic frequency content (high entropy) Dissection has more
  // structured, melodic frequency content (lower entropy)
  // Higher similarity = songs have similar levels of spectral predictability
  Algorithm *entropy;
  // Zero Crossing Rate: How often the signal crosses the zero line
  // Example: Comparing Anaal Nathrakh's "In the Constellation of the Black
  // Widow" vs. Wolves in the Throne Room's "Two Hunters"
  // Anaal Nathrakh has  extremely high ZCR due to their hyper-compressed,
  // digital production style with lots of high-frequency content and distortion
  // Wolves in the Throne Room has lower ZCR due to their more analog,
  // atmospheric production with emphasis on midrange frequencies
  // Higher similarity = songs have similar levels of signal oscillation and
  // high-frequency content
  Algorithm *zcr;
  // Mel-bands Crest: The peak-to-average ratio across frequency bands that
  // approximate human hearing
  // Example: Comparing Revenge's "Behold.Total.Rejection" vs. Emperor's "In the
  // Nightside Eclipse"
  // Revenge has very high crest factor due to extreme
  // dynamics between blast beats and noise Emperor has lower crest factor due
  // to more consistent, layered orchestration
  // Higher similarity = songs have similar dynamic range characteristics in
  // their frequency bands
  Algorithm *mel_bands;
  // Pitch Salience: How "strong" or "clear" the predominant pitch is
  // Example: Comparing Devin Townsend's "Deconstruction" vs. Darkspace's "Dark
  // Space III"
  // Devin Townsend has higher pitch salience due to clear melodic
  // elements despite complexity Darkspace has lower pitch salience due to their
  // ambient, pitch-obscured approach
  // Higher similarity = songs have similar levels of pitch clarity/predominance
  Algorithm *pitch_salience;
  // Chord Descriptors: Two outputs are used from this algorithm.
  //
  // Chord Number Rate: How consistent/stable the harmonic content remains over
  // time
  // Example: Comparing Ihsahn's "After" vs. Peste Noire's "La Chaise-Dyable"
  // Ihsahn has higher chord stability due to his progressive
  // approach with clear, held chord progressions and jazz influences Peste
  // Noire has lower stability due to their chaotic, folk-influenced approach
  // with rapid chord changes and discordant elements
  // Higher similarity = songs have similar levels of harmonic consistency
  //
  // Chord Changes Rate: How frequently the chords change
  // Example: Comparing Between the Buried and Me's "Colors" vs. Inquisition's
  // "Obscure Verses for the Multiverse"
  // BTBAM has very high chord change rate
  // due to their progressive approach with complex harmonic movements
  // Inquisition has lower chord change rate, focusing on hypnotic,
  // repeated riff structures
  // Higher similarity = songs have similar harmonic rhythm
  Algorithm *chord_descriptors;
  Algorithm *chord_detection;
  Algorithm *key_extractor;
  // HPCP Crest: Peak-to-average ratio of the harmonic pitch class profile
  // Example: Comparing Enslaved's "RIITIIR" vs. Teitanblood's "Death"
  // Enslaved has higher HPCP crest due to clear, prominent progressive riffs
  // that create distinct peaks in specific pitch classes
  // Teitanblood has lower HPCP crest due to their murky, pitch-obscured
  // death metal approach that blurs pitch class distinction
  // Higher similarity = songs have similar harmonic clarity and definition
  Algorithm *hpcp_crest;
  Algorithm *spectral_peaks;
  // There are two outputs used from this algorithm.
  //
  // Dynamic Complexity: How much the loudness changes over time
  // Example: Comparing Opeth's "Blackwater Park" vs. Revenge's
  // "Scum.Collapse.Eradication"
  // Opeth has high dynamic complexity due to their
  // frequent transitions between soft passages and heavy sections Revenge has
  // lower dynamic complexity due to their constant full-force assault with
  // minimal dynamic variation
  // Higher similarity = songs have similar patterns of loudness variation
  //
  // Estimate Loudness: The overall perceived loudness level
  // Example: Comparing Strapping Young Lad's "Alien" vs. Weakling's "Dead as
  // Dreams"
  // SYL has extremely high average loudness due to Devin's "wall of
  // sound" production and maximalist approach Weakling has lower average
  // loudness with more dynamic range and atmospheric space
  // Higher similarity = songs have similar overall perceived volume levels
  Algorithm *dynamic_complexity;
  // There is two outputs used from this algorithm.
  // BPM (Beats Per Minute): The tempo of the track
  // Example: Comparing Origin's "Informis Infinitas Inhumanitas" vs. Celtic
  // Frost's "Dawn of Meggido"
  // Origin operates at extremely high BPM (often
  // >250) due to their technical brutal death metal style Celtic Frost
  // maintains slower, doom-influenced tempos (often <100 BPM)
  // Higher similarity = songs have similar overall tempo
  //
  // Beats Count (calculated by taking the size of estimated tick location
  // vector): Total number of detected beats in the track
  // Example: Comparing Meshuggah's "Bleed" vs. Sunn O)))'s "It Took the Night
  // to Believe" Meshuggah has very high beat count due to their polyrhythmic,
  // rapid-fire mechanical precision drumming Sunn O))) has extremely low beat
  // count due to their drone approach with minimal traditional percussion
  // Higher similarity = songs have similar rhythmic density
  Algorithm *rhythm_extractor;
  // Onset Rate: How frequently new musical events/attacks occur
  // Example: Comparing Cryptopsy's "None So Vile" vs. Earth's "Earth 2"
  // Cryptopsy has very high onset rate due to their technical death metal style
  // with constant rhythmic attacks and changes
  // Earth has minimal onset rate due to their drone doom approach with
  // gradual, flowing changes
  // Higher similarity = songs have similar rates of musical events
  Algorithm *onset_rate;
  // Danceability: How suitable the rhythm is for dancing (despite being metal!)
  // Example: Comparing Gojira's "From Mars to Sirius" vs. Portal's "Outre"
  // Gojira has higher danceability due to their groovy, syncopated rhythms
  // Portal has very low danceability due to their abstract, arhythmic approach
  // Higher similarity = songs have similar rhythmic regularity/predictability
  Algorithm *danceability;
  sqlite3 *database{nullptr};

  Algorithm *frameCutter;
  Algorithm *window;
  const char *discogs_effnet_b64_filename;
  Algorithm *discogs_effnet_b64;
};

struct SimilarTrack {
  const char *artist;
  const char *title;
  const char *path;
};

struct TrackFeatures {
public:
  float onset_rate;
  std::vector<float> spectral_centroid;
  std::vector<float> spectral_kurtosis;
  std::vector<float> spectral_skewness;
  std::vector<float> spectral_spread;
  std::vector<float> spectral_complexity;
  float entropy;
  std::vector<float> flux;
  float zeroCrossingRate;
  std::vector<std::vector<float>> melbands;
  std::vector<float> pitchSalience;
  float chordsNumberRate;
  float chordsChangesRate;
  std::vector<std::vector<float>> hpcp;
  float dynamicComplexity;
  float loudness;
  float bpm;
  float beats_count;
  float onsetRate;
  float danceability;

  NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(TrackFeatures, onset_rate,
                                     spectral_centroid, spectral_kurtosis,
                                     spectral_skewness, spectral_spread,
                                     spectral_complexity, entropy, flux,
                                     zeroCrossingRate, melbands, pitchSalience,
                                     chordsNumberRate, chordsChangesRate, hpcp,
                                     spectral_complexity, loudness, bpm,
                                     beats_count, onset_rate);
};
#endif