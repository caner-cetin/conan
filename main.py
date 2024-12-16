import hashlib
import json
import sys
from dataclasses import (
    asdict,
    dataclass,
)
from datetime import datetime
from pathlib import Path
from typing import (
    Dict,
    List,
    Optional,
    Tuple,
)

import essentia.standard as es
import numpy as np
from essentia.standard import MonoLoader
from numpy.typing import NDArray
from PySide6.QtCore import (
    Qt,
    QThread,
    Signal,
    Slot,
)
from PySide6.QtGui import (
    QColor,
    QFont,
    QPalette,
)
from PySide6.QtWidgets import (
    QApplication,
    QFileDialog,
    QGridLayout,
    QGroupBox,
    QHBoxLayout,
    QLabel,
    QListWidget,
    QMainWindow,
    QProgressBar,
    QPushButton,
    QSlider,
    QTabWidget,
    QTextEdit,
    QVBoxLayout,
    QBoxLayout,
    QWidget,
)

from models import Classifier

@dataclass
class AudioFeatures:
    """Enhanced structured container for audio features"""

    bpm: float
    rhythm_strength: float
    rhythm_regularity: float
    danceability: list[float]
    energy: float
    mood_sad: list[float]
    mood_relaxed: list[float]
    mood_aggressive: list[float]
    mirex: list[float]
    genre_probabilities: list[float]
    genre_labels: list[str]
    mfcc_mean: NDArray[np.float32]
    mfcc_var: NDArray[np.float32]
    onset_rate: NDArray[np.float32]
    pitch: float
    loudness: float
    last_modified: str
    file_hash: str

    def to_dict(self) -> dict:
        return asdict(self)

    @classmethod
    def from_dict(cls, data: dict) -> "AudioFeatures":
        return cls(**data)


class AnalysisWorker(QThread):
    progress = Signal(float)
    track_analyzed = Signal(str, object)
    finished = Signal()

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
        self.loader = MonoLoader()

    def _get_file_hash(self, file_path: Path) -> str:
        hasher = hashlib.md5()
        with open(file_path, "rb") as f:
            buf = f.read(1024 * 1024)
            hasher.update(buf)
        return hasher.hexdigest()

    def analyze_track(self, audio_path: Path) -> Optional[AudioFeatures]:
        # Load and preprocess audio
        self.loader.configure(
            sampleRate=16000,
            resampleQuality=4,
            filename=str(audio_path),
        )
        audio = self.loader()
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
        bpm, beats, beats_conf, _, beats_loudness = self.rhythm_extractor(audio)
        energy = np.mean(beats_loudness) / 100
        rhythm_strength = np.mean(beats_conf)

        pitch, _ = self.pitch_extractor(audio)
        genre_labels, genre_probabilities = self.classifiers.genre(audio)

        mfcc_bands, mfcc_coeffs = self.mfcc(audio)
        onset_rate, onset_times = self.onset_rate(audio)
        return AudioFeatures(
            bpm=float(bpm),
            rhythm_strength=float(rhythm_strength),
            rhythm_regularity=float(np.std(np.diff(beats))),
            danceability=self.classifiers.danceability(audio),
            energy=float(energy),
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
            mfcc_mean=np.mean(mfcc_coeffs, axis=0).item(),
            mfcc_var=np.var(mfcc_coeffs, axis=0).item(),
            onset_rate=onset_rate,
        )

    def run(self):
        audio_patterns = ["*.mp3", "*.wav", "*.flac", "*.m4a"]
        audio_files = []
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


class WeightSlider(QWidget):
    def __init__(self, label: str, parent=None):
        super().__init__(parent)
        layout = QHBoxLayout()
        self.label = QLabel(label)
        self.slider = QSlider(Qt.Horizontal)
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


class MusicAnalyzer(QMainWindow):
    def __init__(self):
        super().__init__()
        self.tracks_features: Dict[str, AudioFeatures] = {}
        self.setup_ui()
        self.apply_styles()

    def setup_ui(self):
        self.setWindowTitle("Music Analyzer")
        self.setMinimumSize(1200, 800)

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

        # Left side - Track list
        track_list_container = QVBoxLayout()
        self.track_filter = QTextEdit()
        self.track_filter.setMaximumHeight(30)
        self.track_filter.setPlaceholderText("Filter tracks...")
        self.tracks_list = QListWidget()

        track_list_container.addWidget(self.track_filter)
        track_list_container.addWidget(self.tracks_list)

        # Right side - Features and similar tracks
        self.tabs = QTabWidget()
        self.features_text = QTextEdit()
        self.similar_tracks_text = QTextEdit()
        self.features_text.setReadOnly(True)
        self.similar_tracks_text.setReadOnly(True)

        self.tabs.addTab(self.features_text, "Features")
        self.tabs.addTab(self.similar_tracks_text, "Similar Tracks")

        # Weights configuration
        weights_group = QGroupBox("Similarity Weights")
        weights_layout = QGridLayout()

        self.weight_sliders = {
            "rhythm": WeightSlider("Rhythm"),
            "tonal": WeightSlider("Tonal"),
            "energy": WeightSlider("Energy"),
            "mood": WeightSlider("Mood"),
            "genre": WeightSlider("Genre"),
            "timbral": WeightSlider("Timbral"),
        }

        for i, (name, slider) in enumerate(self.weight_sliders.items()):
            weights_layout.addWidget(slider, i // 2, i % 2)

        weights_group.setLayout(weights_layout)

        # Add everything to the main layout
        content_layout.addLayout(track_list_container, 1)
        content_layout.addWidget(self.tabs, 2)

        main_layout.addLayout(toolbar)
        main_layout.addWidget(self.progress_bar)
        main_layout.addLayout(content_layout)
        main_layout.addWidget(weights_group)

        # Connect signals
        self.select_dir_btn.clicked.connect(self.select_directory)
        self.load_analysis_btn.clicked.connect(self.load_analysis)
        self.save_analysis_btn.clicked.connect(self.save_analysis)
        self.start_analysis_btn.clicked.connect(self.start_analysis)
        self.stop_analysis_btn.clicked.connect(self.stop_analysis)
        self.tracks_list.currentItemChanged.connect(self.on_track_selected)
        self.track_filter.textChanged.connect(self.filter_tracks)

    def calculate_genre_similarity(
        self, base_genres: Dict[str, float], other_genres: Dict[str, float]
    ) -> float:
        all_genres = set(base_genres.keys()) | set(other_genres.keys())
        base_vector = np.array([base_genres.get(genre, 0.0) for genre in all_genres])
        other_vector = np.array([other_genres.get(genre, 0.0) for genre in all_genres])

        base_norm = np.linalg.norm(base_vector)
        other_norm = np.linalg.norm(other_vector)

        if base_norm > 0:
            base_vector = base_vector / base_norm
        if other_norm > 0:
            other_vector = other_vector / other_norm

        dot_product = np.dot(base_vector, other_vector)

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
    ) -> List[Tuple[str, float]]:
        if track_path not in self.tracks_features:
            return []

        base_features = self.tracks_features[track_path]
        similarities = []

        # Get weights from sliders
        w_rhythm = self.weight_sliders["rhythm"].value()
        w_tonal = self.weight_sliders["tonal"].value()
        w_energy = self.weight_sliders["energy"].value()
        w_mood = self.weight_sliders["mood"].value()
        w_genre = self.weight_sliders["genre"].value()
        w_timbral = self.weight_sliders["timbral"].value()

        for other_path, other_features in self.tracks_features.items():
            if other_path == track_path:
                continue

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
                    abs(base_features.bpm - other_features.bpm) / 200,
                    abs(base_features.rhythm_strength - other_features.rhythm_strength),
                ]
            )

            timbral_dist = w_timbral * np.mean(
                [
                    np.mean(np.abs(base_features.mfcc_mean - other_features.mfcc_mean)),
                    np.mean(np.abs(base_features.mfcc_var - other_features.mfcc_var)),
                ]
            )

            energy_dist = w_energy * abs(base_features.energy - other_features.energy)

            mirex_dist = np.mean(
                [abs(a - b) for a, b in zip(base_features.mirex, other_features.mirex)]
            )

            tonal_dist = w_tonal * np.mean(
                [abs(base_features.pitch - other_features.pitch), mirex_dist]
            )

            mood_dist = w_mood * np.mean(
                [
                    abs(base_features.mood_sad[0] - other_features.mood_sad[0]),
                    abs(base_features.mood_relaxed[0] - other_features.mood_relaxed[0]),
                    abs(
                        base_features.mood_aggressive[0]
                        - other_features.mood_aggressive[0]
                    ),
                    abs(base_features.danceability[0] - other_features.danceability[0]),
                ]
            )

            # Artist familiarity bonus
            artist_bonus = (
                0.2
                if other_features.last_modified == base_features.last_modified
                else 0
            )

            total_distance = (
                rhythm_dist
                + tonal_dist
                + energy_dist
                + mood_dist
                + genre_dist
                + timbral_dist
            ) / (w_rhythm + w_tonal + w_energy + w_mood + w_genre + w_timbral)

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
            QFileDialog.ShowDirsOnly | QFileDialog.DontResolveSymlinks,
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

    @Slot(float)
    def update_progress(self, progress: float):
        self.progress_bar.setValue(int(progress * 100))

    @Slot(str, object)
    def on_track_analyzed(self, path: str, features: AudioFeatures):
        self.tracks_features[path] = features
        self.tracks_list.addItem(Path(path).name)

    @Slot()
    def on_analysis_finished(self):
        self.start_analysis_btn.setEnabled(True)
        self.stop_analysis_btn.setEnabled(False)
        self.progress_bar.setValue(100)

    @Slot()
    def on_track_selected(self, current, previous):
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

            # Update features text
            features_text = (
                "====================\n"
                "Basic Info\n"
                "====================\n"
                f"BPM: {features.bpm:.1f}\n"
                f"Energy: {features.energy:.2f}\n"
                "====================\n"
                "Genre Probabilities\n"
                "====================\n"
            )

            features_text += "\n".join(
                f"{label}: {prob:.2f}"
                for label, prob in zip(
                    features.genre_labels, features.genre_probabilities
                )
            )

            features_text += (
                "\n====================\n"
                "Mood Analysis\n"
                "====================\n"
                f"Sad: {features.mood_sad[0]:.2f}\n"
                f"Relaxed: {features.mood_relaxed[0]:.2f}\n"
                f"Aggressive: {features.mood_aggressive[0]:.2f}\n"
                f"Danceable: {features.danceability[0]:.2f}\n"
                "\nMIREX Mood Categories:\n"
                f"Passionate/Rousing: {features.mirex[0]:.2f}\n"
                f"Cheerful/Fun: {features.mirex[1]:.2f}\n"
                f"Brooding/Poignant: {features.mirex[2]:.2f}\n"
                f"Humorous/Witty: {features.mirex[3]:.2f}\n"
                f"Aggressive/Intense: {features.mirex[4]:.2f}\n"
                "====================\n"
                "Rhythm\n"
                "====================\n"
                f"Strength: {features.rhythm_strength:.2f}\n"
                f"Regularity: {features.rhythm_regularity:.2f}\n"
            )

            self.features_text.setText(features_text)

            # Update similar tracks
            similar_tracks = self.get_similar_tracks(selected_path)
            similar_text = "Similar Tracks:\n" + "\n".join(
                f"{Path(path).name}: %{abs(sim):.3f}" for path, sim in similar_tracks
            )
            self.similar_tracks_text.setText(similar_text)

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
        style = """
        QMainWindow, QWidget {
            background-color: #110c22;
            color: #c3dce5;
            font-family: "Courier New", monospace;
        }
        QPushButton {
            background-color: #110c22;
            border: 1px solid #c3dce5;
            padding: 4px 8px;
            color: #c3dce5;
            min-height: 20px;
        }
        QPushButton:hover {
            background-color: #c3dce5;
            color: #110c22;
        }
        QProgressBar {
            border: 1px solid #c3dce5;
            text-align: center;
            color: #c3dce5;
            max-height: 10px;
        }
        QProgressBar::chunk {
            background-color: #c3dce5;
        }
        QListWidget {
            background-color: #110c22;
            border: 1px solid #c3dce5;
            font-size: 11px;
        }
        QListWidget::item {
            padding: 2px;
        }
        QListWidget::item:selected {
            background-color: #c3dce5;
            color: #110c22;
        }
        QTextEdit {
            background-color: #110c22;
            border: 1px solid #c3dce5;
            padding: 4px;
            font-size: 11px;
        }
        QTabWidget::pane {
            border: 1px solid #c3dce5;
            background-color: #110c22;
        }
        QTabBar::tab {
            background-color: #110c22;
            border: 1px solid #c3dce5;
            border-bottom: none;
            padding: 4px 8px;
            margin-right: -1px;
        }
        QTabBar::tab:selected {
            background-color: #c3dce5;
            color: #110c22;
        }
        QGroupBox {
            border: 1px solid #c3dce5;
            margin-top: 4px;
            padding-top: 8px;
            font-size: 11px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 8px;
            padding: 0 2px;
        }
        QSlider::groove:horizontal {
            border: 1px solid #c3dce5;
            height: 4px;
            background: #110c22;
        }
        QSlider::handle:horizontal {
            background: #c3dce5;
            border: 1px solid #c3dce5;
            width: 8px;
            margin: -4px 0;
        }
        QScrollBar {
            background-color: #110c22;
            width: 10px;
        }
        QScrollBar::handle {
            background-color: #c3dce5;
            min-height: 20px;
        }
        QScrollBar::add-line, QScrollBar::sub-line {
            background: none;
        }
        """
        self.setStyleSheet(style)

        # Update layout spacing
        for layout in self.findChildren(QBoxLayout):
            layout.setSpacing(2)
            layout.setContentsMargins(2, 2, 2, 2)

    # [Rest of the implementation including analysis, file handling, etc.]
    # The core functionality methods remain similar to the original implementation
    # but adapted to work with PySide6 signals and slots


if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = MusicAnalyzer()
    window.show()
    sys.exit(app.exec())
