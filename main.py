import hashlib
import json
import sys
import threading
from datetime import datetime
from pathlib import Path
from typing import final

import essentia.standard as es
import numpy as np
from dotenv import load_dotenv
from numpy.typing import NDArray
from PySide6.QtCore import (
    Qt,
    QThread,
    Signal,
    Slot,
)
from PySide6.QtWidgets import (
    QApplication,
    QFileDialog,
    QGridLayout,
    QGroupBox,
    QHBoxLayout,
    QLabel,
    QListWidget,
    QListWidgetItem,
    QMainWindow,
    QProgressBar,
    QPushButton,
    QSlider,
    QTextEdit,
    QVBoxLayout,
    QWidget,
)
from tinytag import TinyTag
from typing_extensions import override

from backend import run_server
from constants import AudioFeatures
from models import Classifier
from player import Foobar2K, TrackInfo
from templates import TrackDisplayTemplate


@final
class AnalysisWorker(QThread):
    progress: Signal = Signal(float)
    track_analyzed: Signal = Signal(str, object)

    def __init__(self, music_dir: str):
        super().__init__()
        self.music_dir = music_dir
        self.analyzing = True
        self.init_analyzers()
        self.classifiers = Classifier()

    def init_analyzers(self):
        self.rhythm_extractor = es.RhythmExtractor2013()
        self.pitch_extractor = es.PredominantPitchMelodia()
        self.loudness = es.Loudness()
        self.mfcc = es.MFCC()
        self.onset_rate = es.OnsetRate()
        self.dc_remover = es.DCRemoval()
        self.equalizer = es.EqualLoudness()
        self.bandpass = es.BandPass(
            cutoffFrequency=2000,
            bandwidth=19980,
            sampleRate=44100,
        )
        self.moving_average = es.MovingAverage()
        self.loader = es.MonoLoader()

    def _get_file_hash(self, file_path: Path) -> str:
        hasher = hashlib.md5()
        with open(file_path, "rb") as f:
            buf = f.read(1024 * 1024)
            hasher.update(buf)
        return hasher.hexdigest()

    def analyze_track(self, audio_path: Path) -> AudioFeatures:
        # Load and preprocess audio
        self.loader.configure(
            sampleRate=16000,
            resampleQuality=4,
            filename=str(audio_path),
        )
        audio: NDArray[np.float32] = self.loader()
        self.loader.configure(
            sampleRate=44100, resampleQuality=4, filename=str(audio_path)
        )
        audio44k: NDArray[np.float32] = self.loader()
        # short explanation: remove the DC component from the audio signal for removing the noise from the audio.
        # long explanation by claude:
        #
        # DC offset is basically a constant shift in the audio signal that's at 0 Hz (meaning it doesn't change/vibrate at all), and human ears can only hear sounds between roughly 20 Hz and 20,000 Hz.
        # But DC offset can cause two big problems:
        # - It wastes headroom in your audio signal. Think of it like trying to fit a tall person in a room with a low ceiling - if they're standing on a platform (DC offset),
        #   they might hit their head. Similarly, if your audio wave is shifted up or down, you have less room for the actual audio signal before it distorts.
        # - It can mess up your audio analysis algorithms. Many audio analysis tools (like those in Essentia) expect the audio to be centered around zero.
        #   If there's DC offset, it can throw off measurements of things like amplitude, energy, and other features you might want to analyze.
        audio = self.dc_remover(audio)
        # short explanation: equalize the audio for mimicing the human ears.
        # long explanation by claude:
        #
        # Think about how your ears work. When you listen to music, your ears don't hear all frequencies equally well - they're naturally better at hearing some frequencies than others.
        # For example, you hear mid-range frequencies (like people talking) much better than very low or very high frequencies.
        # The equal loudness filter tries to mimic how human ears work. It boosts or reduces different frequencies to match how we actually hear them.
        # It's like having a special pair of headphones that automatically adjusts different sounds to match how your ears naturally process them.
        # Example:
        # - Without the filter: A very low bass note and a mid-range piano note might have the same volume in the audio file
        # - With the filter: The bass note gets boosted because our ears naturally hear it as quieter than it actually is
        audio = self.equalizer(audio)
        # short explanation: weakent the frequencies outside of 20 Hz to 20 KHz frequency range for stripping audio to coverage of human ear.
        # kinda long explanation by me:
        # by stripping noise outside of the frequencies capable of human ear, we assure that algorithms and models actually run on the data that is relevant to the human ear, such as
        # Low bass frequencies (important for genre detection)
        # High frequencies (important for detecting cymbals, hi-hats, etc.)
        # Mid-range frequencies (important for detecting vocals, guitars, etc.)
        audio = self.bandpass(audio)
        # short explanation: i dont know
        # short explanation from my friend: smooths out the signal, it makes the analysis better trust me
        # long explanation by claude: claude doesnt know either
        # it works tho
        audio = self.moving_average(audio)

        # Rhythm analysis
        # according to https://essentia.upf.edu/reference/std_RhythmExtractor2013.html
        bpm, beats, beats_conf, _, _ = self.rhythm_extractor(audio)
        rhythm_strength = np.mean(beats_conf)

        pitch, _ = self.pitch_extractor(audio)
        genre_labels, genre_probabilities = self.classifiers.genre(audio)

        _, mfcc_coeffs = self.mfcc(audio)
        # https://essentia.upf.edu/reference/streaming_OnsetRate.html
        # Please note that due to a dependence on the Onsets algorithm, this algorithm is only valid for audio signals with a sampling rate of 44100Hz.
        # This algorithm throws an exception if the input signal is empty.
        onset_rate, _ = self.onset_rate(audio44k)

        tag: TinyTag = TinyTag.get(audio_path)  # pyright: ignore[reportUnknownMemberType]
        return AudioFeatures(
            bpm=float(bpm),
            rhythm_strength=float(rhythm_strength),
            rhythm_regularity=float(np.std(np.diff(beats))),
            danceability=self.classifiers.danceability(audio),
            mood_sad=self.classifiers.sad(audio),
            mood_relaxed=self.classifiers.relaxed(audio),
            mood_aggressive=self.classifiers.aggressive(audio),
            last_modified=datetime.fromtimestamp(
                audio_path.stat().st_mtime
            ).isoformat(),
            file_hash=self._get_file_hash(audio_path),
            genre_probabilities=genre_probabilities,
            genre_labels=genre_labels,
            loudness=self.loudness(audio),
            pitch=np.mean(pitch).item(),
            mirex=self.classifiers.mirex(audio),
            mfcc_mean=np.mean(mfcc_coeffs, axis=0).tolist(),
            mfcc_var=np.var(mfcc_coeffs, axis=0).tolist(),
            onset_rate=onset_rate.tolist(),
            metadata=tag.as_dict(),
            instrumental=self.classifiers.instrumental_or_vocal(audio),
            engagement=self.classifiers.engaging(audio),
            vocal_gender=self.classifiers.vocal_gender(audio),
        )

    @override
    def run(self):
        audio_patterns = ["*.mp3", "*.wav", "*.flac", "*.m4a"]
        audio_files: list[Path] = []
        for pattern in audio_patterns:
            audio_files.extend(Path(self.music_dir).glob(f"**/{pattern}"))

        total_files = len(audio_files)
        for i, audio_file in enumerate(audio_files):
            if not self.analyzing:
                break

            features = self.analyze_track(audio_file)
            if features:
                self.track_analyzed.emit(str(audio_file), features)

            self.progress.emit(i / total_files)

        self.finished.emit()


@final
class WeightSlider(QWidget):
    def __init__(self, label: str, parent: QWidget | None = None):
        super().__init__(parent)
        layout = QHBoxLayout()
        self.label = QLabel(label)
        self.slider = QSlider(Qt.Orientation.Horizontal)
        self.slider.setRange(0, 200)
        self.slider.setValue(100)
        self.value_label = QLabel("1.0")

        self.slider.valueChanged.connect(self._update_value)

        layout.addWidget(self.label)
        layout.addWidget(self.slider)
        layout.addWidget(self.value_label)
        self.setLayout(layout)

    def _update_value(self):
        value = self.slider.value() / 100
        self.value_label.setText(f"{value:.1f}")

    def value(self) -> float:
        return self.slider.value() / 100


@final
class MusicAnalyzer(QMainWindow):
    def __init__(self, app: QApplication):
        super().__init__()
        self.tracks_features: dict[str, AudioFeatures] = {}
        self.app = app
        self.f2k = Foobar2K(self)
        self.app.aboutToQuit.connect(self.f2k.cleanup)
        self.setup_ui()
        self.apply_styles()

    def setup_ui(self):
        self.setWindowTitle("Conan")
        self.setMinimumSize(1600, 900)

        # Create central widget and main layout
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        main_layout = QVBoxLayout(central_widget)

        # Create top toolbar
        toolbar = QHBoxLayout()
        self.select_dir_btn = QPushButton("Select Music Directory")
        self.load_analysis_btn = QPushButton("Load Analysis")
        self.save_analysis_btn = QPushButton("Save Analysis")
        self.start_analysis_btn = QPushButton("Start Analysis")
        self.stop_analysis_btn = QPushButton("Stop Analysis")

        toolbar.addWidget(self.select_dir_btn)
        toolbar.addWidget(self.load_analysis_btn)
        toolbar.addWidget(self.save_analysis_btn)
        toolbar.addWidget(self.start_analysis_btn)
        toolbar.addWidget(self.stop_analysis_btn)

        # Create progress bar
        self.progress_bar = QProgressBar()

        # Create main content area
        content_layout = QHBoxLayout()

        # Left side - Track list & Foobar2000
        self.track_list_container = QVBoxLayout()
        self.track_filter = QTextEdit()
        self.track_filter.setMaximumHeight(30)
        self.track_filter.setPlaceholderText("Filter tracks...")
        self.tracks_list = QListWidget()

        self.track_list_container.addWidget(self.track_filter)
        self.track_list_container.addWidget(self.tracks_list)
        self.track_list_container.addLayout(self.f2k.player_layout)

        self.track_info = TrackInfo()

        # Weights configuration
        weights_group = QGroupBox("Similarity Weights")
        weights_layout = QGridLayout()

        self.weight_sliders = {
            "rhythm": WeightSlider("Rhythm"),
            "tonal": WeightSlider("Tonal"),
            "instrumental": WeightSlider("Instrumental"),
            "mood": WeightSlider("Mood"),
            "genre": WeightSlider("Genre"),
            "timbral": WeightSlider("Timbral"),
        }

        for i, (_, slider) in enumerate(self.weight_sliders.items()):
            weights_layout.addWidget(slider, i // 2, i % 2)

        weights_group.setLayout(weights_layout)
        self.track_list_container.addWidget(weights_group)

        # Add everything to the main layout
        content_layout.addLayout(self.track_list_container, 1)
        content_layout.addWidget(self.track_info, 2)
        main_layout.addLayout(toolbar)
        main_layout.addWidget(self.progress_bar)
        main_layout.addLayout(content_layout)

        # Connect signals
        self.select_dir_btn.clicked.connect(self.select_directory)
        self.load_analysis_btn.clicked.connect(self.load_analysis)
        self.save_analysis_btn.clicked.connect(self.save_analysis)
        self.start_analysis_btn.clicked.connect(self.start_analysis)
        self.stop_analysis_btn.clicked.connect(self.stop_analysis)
        self.tracks_list.currentItemChanged.connect(self.on_track_selected)
        self.track_filter.textChanged.connect(self.filter_tracks)

    def calculate_genre_similarity(
        self, base_genres: dict[str, float], other_genres: dict[str, float]
    ) -> float:
        all_genres = set(base_genres.keys()) | set(other_genres.keys())
        base_vector: NDArray[np.float32] = np.array(
            [base_genres.get(genre, 0.0) for genre in all_genres]
        )
        other_vector: NDArray[np.float32] = np.array(
            [other_genres.get(genre, 0.0) for genre in all_genres]
        )

        base_norm = np.linalg.norm(base_vector)
        other_norm = np.linalg.norm(other_vector)

        if base_norm > 0:
            base_vector = base_vector / base_norm
        if other_norm > 0:
            other_vector = other_vector / other_norm

        dot_product: NDArray[np.float32] = np.dot(base_vector, other_vector)

        N = 3
        base_top = sorted(base_genres.items(), key=lambda x: x[1], reverse=True)[:N]
        other_top = sorted(other_genres.items(), key=lambda x: x[1], reverse=True)[:N]

        base_top_genres = set(g[0] for g in base_top)
        other_top_genres = set(g[0] for g in other_top)
        genre_overlap = len(base_top_genres & other_top_genres)
        overlap_bonus = genre_overlap / N

        return (dot_product + overlap_bonus) / 2

    def get_similar_tracks(
        self, track_path: str, n: int = 5
    ) -> list[tuple[str, float]]:
        if track_path not in self.tracks_features:
            return []

        base_features = self.tracks_features[track_path]
        similarities = []

        # Get weights from sliders
        w_rhythm = self.weight_sliders["rhythm"].value()
        w_tonal = self.weight_sliders["tonal"].value()
        w_instrumental = self.weight_sliders["instrumental"].value()
        w_mood = self.weight_sliders["mood"].value()
        w_genre = self.weight_sliders["genre"].value()
        w_timbral = self.weight_sliders["timbral"].value()

        artist_seen: dict[str, int] = {}

        for other_path, other_features in self.tracks_features.items():
            if other_path == track_path:
                continue

            if isinstance(base_features.metadata["artist"], list) and isinstance(
                other_features.metadata["artist"], list
            ):
                k = "".join(base_features.metadata["artist"])
                seen = artist_seen.get(k, 0)
                if seen >= 3:
                    continue
                if seen == 0:
                    artist_seen[k] = 1
                else:
                    artist_seen[k] += 1
                artist_bonus = (
                    0.2
                    if other_features.metadata["artist"][0]
                    == base_features.metadata["artist"][0]
                    else 0
                )
            else:
                artist_bonus = 0

            genre_sim = self.calculate_genre_similarity(
                dict(
                    zip(base_features.genre_labels, base_features.genre_probabilities)
                ),
                dict(
                    zip(other_features.genre_labels, other_features.genre_probabilities)
                ),
            )
            genre_dist = w_genre * (1 - genre_sim)

            rhythm_dist = w_rhythm * np.mean(
                [
                    np.abs(base_features.bpm - other_features.bpm) / 200,
                    np.abs(
                        base_features.rhythm_strength - other_features.rhythm_strength
                    ),
                    np.mean(
                        np.abs(
                            np.subtract(
                                base_features.engagement, other_features.engagement
                            )
                        )
                    ),
                ]
            )

            timbral_dist = w_timbral * np.mean(
                [
                    np.mean(
                        np.abs(
                            np.subtract(
                                base_features.mfcc_mean, other_features.mfcc_mean
                            )
                        )
                    ),
                    np.mean(
                        np.abs(
                            np.subtract(base_features.mfcc_var, other_features.mfcc_var)
                        )
                    ),
                ]
            )

            instrumental_dist = w_instrumental * np.mean(
                np.abs(
                    np.subtract(base_features.instrumental, other_features.instrumental)
                )
            )

            tonal_dist = w_tonal * np.mean(
                [
                    np.abs(base_features.pitch - other_features.pitch),
                    np.mean(
                        np.abs(np.subtract(base_features.mirex, other_features.mirex))
                    ),
                    np.mean(
                        np.abs(
                            np.subtract(
                                base_features.vocal_gender, other_features.vocal_gender
                            )
                        )
                    ),
                ]
            )

            mood_dist = w_mood * np.mean(
                [
                    np.abs(
                        np.subtract(base_features.mood_sad, other_features.mood_sad)
                    ),
                    np.abs(
                        np.subtract(
                            base_features.mood_relaxed,
                            other_features.mood_relaxed,
                        )
                    ),
                    np.abs(
                        np.subtract(
                            base_features.mood_aggressive,
                            other_features.mood_aggressive,
                        )
                    ),
                    np.abs(
                        np.subtract(
                            base_features.danceability, other_features.danceability
                        )
                    ),
                ]
            )

            total_distance = (
                rhythm_dist
                + tonal_dist
                + instrumental_dist
                + mood_dist
                + genre_dist
                + timbral_dist
            ) / (w_rhythm + w_tonal + w_instrumental + w_mood + w_genre + w_timbral)

            total_distance = max(0, total_distance - artist_bonus)
            similarities.append(
                (other_path, 1 - total_distance)
            )  # Convert distance to similarity

        similarities.sort(key=lambda x: x[1], reverse=True)
        return similarities[:n]

    @Slot()
    def select_directory(self):
        dir_path = QFileDialog.getExistingDirectory(
            self,
            "Select Music Directory",
            "",
            QFileDialog.Option.ShowDirsOnly | QFileDialog.Option.DontResolveSymlinks,
        )
        if dir_path:
            self.music_dir = dir_path

    @Slot()
    def start_analysis(self):
        if not hasattr(self, "music_dir") or not self.music_dir:
            return

        self.worker = AnalysisWorker(self.music_dir)
        self.worker.progress.connect(self.update_progress)
        self.worker.track_analyzed.connect(self.on_track_analyzed)
        self.worker.finished.connect(self.on_analysis_finished)
        self.worker.start()

        self.start_analysis_btn.setEnabled(False)
        self.stop_analysis_btn.setEnabled(True)

    @Slot()
    def stop_analysis(self):
        if self.worker:
            self.worker.analyzing = False
            self.worker.wait()
            self.start_analysis_btn.setEnabled(True)
            self.stop_analysis_btn.setEnabled(False)

    @Slot(float)  # pyright: ignore[reportArgumentType]
    def update_progress(self, progress: float) -> None:
        self.progress_bar.setValue(int(progress * 100))

    @Slot(str, AudioFeatures)  # pyright: ignore[reportArgumentType]
    def on_track_analyzed(self, path: str, features: AudioFeatures):
        artist_name: str

        if isinstance(features.metadata["artist"], list):
            if len(features.metadata["artist"]) > 1:
                artist_name = "-".join(features.metadata["artist"])
            else:
                artist_name = features.metadata["artist"][0]
        elif isinstance(features.metadata["title"], str):
            artist_name = features.metadata["title"]
        else:
            artist_name = "Unknown"

        song_title: str
        if isinstance(features.metadata["title"], list):
            if len(features.metadata["title"]) > 1:
                song_title = "-".join(features.metadata["title"])
            else:
                song_title = features.metadata["title"][0]
        elif isinstance(features.metadata["title"], str):
            song_title = features.metadata["title"]
        else:
            song_title = "Unknown"

        label = f"{artist_name} - {song_title}"
        self.tracks_list.addItem(label)
        self.tracks_features[label] = features

    @Slot()
    def on_analysis_finished(self):
        self.start_analysis_btn.setEnabled(True)
        self.stop_analysis_btn.setEnabled(False)
        self.progress_bar.setValue(100)

    @Slot()
    def on_track_selected(
        self,
        current: QListWidgetItem | None = None,
        previous: QListWidgetItem | None = None,
    ):
        if not current:
            return

        selected_name = current.text()
        selected_path = next(
            (
                path
                for path in self.tracks_features.keys()
                if Path(path).name == selected_name
            ),
            None,
        )

        if selected_path:
            features = self.tracks_features[selected_path]
            similar_tracks = self.get_similar_tracks(selected_path)
            html_content = TrackDisplayTemplate().update_display(
                features, similar_tracks, selected_path
            )
            self.track_info.setHtml(html_content)

    @Slot()
    def save_analysis(self):
        filename, _ = QFileDialog.getSaveFileName(
            self, "Save Analysis", "", "JSON Files (*.json)"
        )
        if filename:
            if not filename.endswith(".json"):
                filename += ".json"
            serialized_features = {
                path: features.to_dict()
                for path, features in self.tracks_features.items()
            }
            with open(filename, "w") as f:
                json.dump(serialized_features, f, indent=2)

    @Slot()
    def load_analysis(self):
        filename, _ = QFileDialog.getOpenFileName(
            self, "Load Analysis", "", "JSON Files (*.json)"
        )
        if filename:
            with open(filename, "r") as f:
                data = json.load(f)
                self.tracks_features = {
                    path: AudioFeatures.from_dict(feat_dict)
                    for path, feat_dict in data.items()
                }

            self.tracks_list.clear()
            self.tracks_list.addItems(
                [Path(p).name for p in self.tracks_features.keys()]
            )

    @Slot()
    def filter_tracks(self):
        filter_text = self.track_filter.toPlainText().lower()
        for i in range(self.tracks_list.count()):
            item = self.tracks_list.item(i)
            item.setHidden(filter_text not in item.text().lower())

    def apply_styles(self):
        # https://lospec.com/palette-list/1-bit-chill
        with open("./style.qss") as f:
            self.setStyleSheet(f.read())


if __name__ == "__main__":
    setenv = load_dotenv()
    if setenv is False:
        raise Exception("environment variables must be set")
    app = QApplication(sys.argv)
    window = MusicAnalyzer(app)
    server_thread = threading.Thread(target=run_server, daemon=True)
    server_thread.start()
    window.show()
    sys.exit(app.exec())
