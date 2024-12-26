import json
import sys
import threading
from pathlib import Path
from typing import final

from dotenv import load_dotenv

setenv = load_dotenv()
if setenv is False:
    raise Exception("environment variables must be set")

import numpy as np
from loguru import logger
from numpy.typing import NDArray
from PySide6.QtCore import (
    QSize,
    Qt,
    QThread,
    Signal,
    Slot,
)
from PySide6.QtGui import QIcon
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
from typing_extensions import override

from backend import run_server
from constants import AudioFeatures
from models import Analyzer
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
        self.analyser = Analyzer()

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

            features = self.analyser.analyze_track(audio_file)
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
        self.setMinimumSize(1700, 900)
        app_icon = QIcon()
        app_icon.addFile("./assets/favicon/16x16.png", QSize(16, 16))
        app_icon.addFile("./assets/favicon/32x32.png", QSize(32, 32))
        app_icon.addFile("./assets/favicon/150x150.png", QSize(150, 150))
        app_icon.addFile("./assets/favicon/192x192.png", QSize(192, 192))
        self.setWindowIcon(app_icon)

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
        self.track_list_container.addWidget(self.f2k.similar_tracks)
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

        return np.mean((dot_product + overlap_bonus) / 2.0).item()

    def get_similar_tracks(
        self, track_path: str, n: int = 5
    ) -> list[tuple[str, float, AudioFeatures]]:
        if track_path not in self.tracks_features:
            logger.debug(
                f"{track_path} not found in the track list for similar track analysis."
            )
            return []
        logger.trace(f"Analyzing similar tracks for: {track_path}")
        base_features = self.tracks_features[track_path]
        similarities: list[tuple[str, float, AudioFeatures]] = []

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
                logger.debug(
                    f"Compared label ({other_path}) is same with base label, skipping..."
                )
                continue
            logger.trace(f"Comparing {track_path} with {other_path}")
            if other_features.metadata.artist is not None:
                k = other_features.metadata.artist
                seen = artist_seen.setdefault(k, 0)
                if seen >= 3:
                    logger.debug(
                        f"There are already three recommendations from the artist {k}, skipping..."
                    )
                    continue
                artist_seen[k] += 1
                artist_bonus = (
                    np.float32(0.2)
                    if other_features.metadata.artist == base_features.metadata.artist
                    else np.float32(0.0)
                )
                logger.trace(f"Similar artist bonus is set to {artist_bonus}")
            else:
                artist_bonus = np.float32(0.0)

            genre_sim = self.calculate_genre_similarity(
                dict(
                    zip(base_features.genre_labels, base_features.genre_probabilities)
                ),
                dict(
                    zip(other_features.genre_labels, other_features.genre_probabilities)
                ),
            )
            logger.trace(f"Genre similarity: {genre_sim}")
            genre_dist = w_genre * (1 - genre_sim)
            logger.trace(f"Genre distance: {genre_dist}")
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
            logger.trace(f"Rhythm distance: {rhythm_dist}")
            timbral_dist = w_timbral * np.mean(
                [
                    np.abs(base_features.mfcc_mean - other_features.mfcc_mean),
                    np.abs(base_features.mfcc_var - other_features.mfcc_var),
                ]
            )
            logger.trace(f"Timbral distance: {timbral_dist}")
            instrumental_dist = w_instrumental * np.mean(
                np.abs(
                    np.subtract(base_features.instrumental, other_features.instrumental)
                )
            )
            logger.trace(f"Instrumental distance: {instrumental_dist}")
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
            logger.trace(f"Tonal distance: {tonal_dist}")
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
            logger.trace(f"Mood distance: {mood_dist}")
            total_distance: np.float32 = (
                rhythm_dist
                + tonal_dist
                + instrumental_dist
                + mood_dist
                + genre_dist
                + timbral_dist
            ) / (w_rhythm + w_tonal + w_instrumental + w_mood + w_genre + w_timbral)
            total_distance_f = max(0.0, total_distance.item() - artist_bonus.item())
            similarity = 1.0 - total_distance_f
            logger.trace(f"Total distance: {total_distance_f}")
            logger.trace(f"Similarity: {similarity}")
            similarities.append(
                (other_path, similarity, other_features)
            )  # Convert distance to similarity

        similarities.sort(key=lambda x: x[1], reverse=True)
        logger.trace(f"Final similarity list: {similarities}")
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
        label = f"{features.metadata.artist if features.metadata.artist else 'Unknown Artist'} - {features.metadata.title if features.metadata.title else 'Unknown Title'}"
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
            similar_tracks = self.get_similar_tracks(selected_path, n=10)
            html_content = TrackDisplayTemplate().update_display(
                features, similar_tracks, selected_path
            )
            self.f2k.similar_tracks.clear_tracks()
            for track in similar_tracks:
                self.f2k.similar_tracks.add_track(track[2])
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
                path: features.model_dump()
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
                    path: AudioFeatures(**feat_dict) for path, feat_dict in data.items()
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
    app = QApplication(sys.argv)
    window = MusicAnalyzer(app)
    server_thread = threading.Thread(target=run_server, daemon=True)
    server_thread.start()
    window.show()
    sys.exit(app.exec())
