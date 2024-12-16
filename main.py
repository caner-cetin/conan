import hashlib
import json
import threading
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

import dearpygui.dearpygui as dpg
import essentia.standard as es
import numpy as np
from essentia.standard import MonoLoader
from numpy.typing import NDArray

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

    # https://essentia.upf.edu/reference/std_PredominantPitchMelodia.html
    pitch: float

    loudness: float

    # Metadata
    last_modified: str
    file_hash: str

    def to_dict(self) -> dict:
        return asdict(self)

    @classmethod
    def from_dict(cls, data: dict) -> "AudioFeatures":
        return cls(**data)


class MusicAnalyzerGUI:
    def __init__(self):
        self.music_dir = ""
        self.tracks_features: Dict[str, AudioFeatures] = {}
        self.analyzing = False
        self.current_progress = 0
        self.total_files = 0

        self.init_analyzers()
        self.setup_gui()

    def init_analyzers(self):
        # Core feature extractors
        self.rhythm_extractor = es.RhythmExtractor2013()
        self.pitch_extractor = es.PredominantPitchMelodia()
        self.loudness = es.Loudness()
        self.mfcc = es.MFCC()
        self.onset_rate = es.OnsetRate()

        # Signal preprocessors
        self.dc_remover = es.DCRemoval()
        self.equalizer = es.EqualLoudness()
        self.bandpass = es.BandPass(
            cutoffFrequency=2000,
            bandwidth=19980,  # 20 hz - 20 khz
            sampleRate=44100,
        )
        self.moving_average = es.MovingAverage()
        self.loader = MonoLoader()
        self.classifiers = Classifier()

    def setup_gui(self):
        dpg.create_context()

        # File dialog callback
        def file_dialog_callback(sender, app_data):
            if app_data["file_path_name"]:
                self.load_analysis(app_data["file_path_name"])

        def directory_dialog_callback(sender, app_data):
            if app_data["file_path_name"]:
                self.music_dir = app_data["file_path_name"]
                dpg.set_value("dir_display", f"Selected Directory: {self.music_dir}")

        # File dialogs
        with dpg.file_dialog(
            directory_selector=True,
            show=False,
            callback=directory_dialog_callback,
            tag="directory_dialog",
        ):
            dpg.add_file_extension("")

        with dpg.file_dialog(
            directory_selector=False,
            show=False,
            callback=file_dialog_callback,
            tag="load_dialog",
        ):
            dpg.add_file_extension(".json", color=(0, 255, 0, 255))

        with dpg.file_dialog(
            directory_selector=False,
            show=False,
            callback=lambda s, a: self.save_analysis(a["file_path_name"]),
            tag="save_dialog",
        ):
            dpg.add_file_extension(".json", color=(0, 255, 0, 255))

        # Main window
        with dpg.window(label="Music Analyzer", tag="primary_window"):
            with dpg.group(horizontal=True):
                dpg.add_button(
                    label="Select Music Directory",
                    callback=lambda: dpg.show_item("directory_dialog"),
                )
                dpg.add_button(
                    label="Load Analysis", callback=lambda: dpg.show_item("load_dialog")
                )
                dpg.add_button(
                    label="Save Analysis", callback=lambda: dpg.show_item("save_dialog")
                )

            dpg.add_text("Selected Directory: None", tag="dir_display")

            with dpg.group(horizontal=True):
                dpg.add_button(label="Start Analysis", callback=self.start_analysis)
                dpg.add_button(label="Stop Analysis", callback=self.stop_analysis)

            dpg.add_progress_bar(
                default_value=0.0, overlay="Progress", tag="progress_bar"
            )

            # Feature importance sliders for similarity matching
            with dpg.collapsing_header(label="Similarity Weights", default_open=False):
                with dpg.table(
                    header_row=True,
                    resizable=True,
                    borders_innerH=True,
                    borders_outerH=True,
                    borders_innerV=True,
                    borders_outerV=True,
                    width=430,
                ):
                    dpg.add_table_column(
                        width=100,
                        width_fixed=True,
                        label="Feature",
                    )
                    dpg.add_table_column(width=300, width_fixed=True, label="Weight")
                    dpg.add_table_column(width=50, width_fixed=True, label="Set To")

                    # Rhythm row
                    with dpg.table_row():
                        dpg.add_text("Rhythm")
                        dpg.add_slider_float(
                            default_value=1.0,
                            min_value=0.0,
                            max_value=2.0,
                            tag="weight_rhythm",
                            width=300,
                        )
                        dpg.add_button(
                            label="Default",
                            callback=lambda: dpg.set_value("weight_rhythm", 1.0),
                        )

                    # Tonal row
                    with dpg.table_row():
                        dpg.add_text("Tonal")
                        dpg.add_slider_float(
                            default_value=1.0,
                            min_value=0.0,
                            max_value=2.0,
                            tag="weight_tonal",
                            width=300,
                        )
                        dpg.add_button(
                            label="Default",
                            callback=lambda: dpg.set_value("weight_tonal", 1.0),
                        )

                    # Energy row
                    with dpg.table_row():
                        dpg.add_text("Energy")
                        dpg.add_slider_float(
                            default_value=1.0,
                            min_value=0.0,
                            max_value=2.0,
                            tag="weight_energy",
                            width=300,
                        )
                        dpg.add_button(
                            label="Default",
                            callback=lambda: dpg.set_value("weight_energy", 1.0),
                        )

                    # Mood row
                    with dpg.table_row():
                        dpg.add_text("Mood")
                        dpg.add_slider_float(
                            default_value=1.0,
                            min_value=0.0,
                            max_value=2.0,
                            tag="weight_mood",
                            width=300,
                        )
                        dpg.add_button(
                            label="Default",
                            callback=lambda: dpg.set_value("weight_mood", 1.35),
                        )

                    with dpg.table_row():
                        dpg.add_text("Genre")
                        dpg.add_slider_float(
                            default_value=1.0,
                            min_value=0.0,
                            max_value=2.0,
                            tag="weight_genre",
                            width=300,
                        )
                        dpg.add_button(
                            label="Default",
                            callback=lambda: dpg.set_value("weight_genre", 1.75),
                        )
                    with dpg.table_row():
                        dpg.add_text("Timbral")
                        dpg.add_slider_float(
                            default_value=1.0,
                            min_value=0.0,
                            max_value=2.0,
                            tag="weight_timbral",
                            width=300,
                        )
                        dpg.add_button(
                            label="Default",
                            callback=lambda: dpg.set_value("weight_timbral", 1.0),
                        )
                dpg.add_button(
                    label="Reset Weights",
                    callback=lambda: [
                        dpg.set_value("weight_rhythm", 1.0),
                        dpg.set_value("weight_tonal", 1.0),
                        dpg.set_value("weight_energy", 1.0),
                        dpg.set_value("weight_mood", 1.0),
                    ],
                )

            with dpg.group(horizontal=True):
                # Track list with filter
                with dpg.group():
                    dpg.add_input_text(
                        label="Filter Tracks",
                        callback=self.filter_tracks,
                        tag="track_filter",
                        width=300,
                    )
                    dpg.add_listbox(
                        label="Tracks",
                        width=300,
                        num_items=20,
                        tag="tracks_list",
                        callback=self.on_track_selected,
                    )

                # Features and similar tracks
                with dpg.group():
                    with dpg.tab_bar():
                        with dpg.tab(label="Features"):
                            dpg.add_text("", tag="features_text")
                        with dpg.tab(label="Similar Tracks"):
                            dpg.add_text("", tag="similar_tracks_text")

        dpg.create_viewport(title="Music Analyzer", width=1000, height=800)
        dpg.setup_dearpygui()
        dpg.show_viewport()
        dpg.set_primary_window("primary_window", True)

    def filter_tracks(self, sender, app_data):
        filter_text = app_data.lower()
        track_names = [
            Path(p).name
            for p in self.tracks_features.keys()
            if filter_text in Path(p).name.lower()
        ]
        dpg.configure_item("tracks_list", items=track_names)

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
            mfcc_mean=np.mean(mfcc_coeffs, axis=0),
            mfcc_var=np.var(mfcc_coeffs, axis=0),
            onset_rate=onset_rate,
        )

    def analyze_library_thread(self):
        audio_patterns = ["*.mp3", "*.wav", "*.flac", "*.m4a"]
        self.analyzing = True
        self.current_progress = 0

        audio_files = []
        for pattern in audio_patterns:
            audio_files.extend(Path(self.music_dir).glob(f"**/{pattern}"))

        self.total_files = len(audio_files)

        for audio_file in audio_files:
            if not self.analyzing:  # Check if analysis was cancelled
                break

            features = self.analyze_track(audio_file)
            if features:
                self.tracks_features[str(audio_file)] = features

            self.current_progress += 1
            progress = self.current_progress / self.total_files
            dpg.set_value("progress_bar", progress)

            # Update tracks list
            track_names = [Path(p).name for p in self.tracks_features.keys()]
            dpg.configure_item("tracks_list", items=track_names)

        self.analyzing = False

    def start_analysis(self):
        if not self.music_dir:
            dpg.set_value("dir_display", "Please select a directory first!")
            return

        if not self.analyzing:
            thread = threading.Thread(target=self.analyze_library_thread)
            thread.start()

    def stop_analysis(self):
        self.analyzing = False

    def calculate_genre_similarity(
        self, base_genres: Dict[str, float], other_genres: Dict[str, float]
    ) -> float:
        """
        Calculate genre similarity between two tracks using a more robust approach.

        Args:
            base_genres: Dictionary of genre probabilities for the base track
            other_genres: Dictionary of genre probabilities for the other track

        Returns:
            float: Similarity score between 0 and 1, where 1 is most similar
        """
        # Create a unified set of all genres
        all_genres = set(base_genres.keys()) | set(other_genres.keys())

        # Create normalized vectors with 0 for missing genres
        base_vector = np.array([base_genres.get(genre, 0.0) for genre in all_genres])
        other_vector = np.array([other_genres.get(genre, 0.0) for genre in all_genres])

        # Normalize the vectors to unit length to focus on proportions rather than absolute values
        base_norm = np.linalg.norm(base_vector)
        other_norm = np.linalg.norm(other_vector)

        if base_norm > 0:
            base_vector = base_vector / base_norm
        if other_norm > 0:
            other_vector = other_vector / other_norm

        # Calculate cosine similarity
        dot_product = np.dot(base_vector, other_vector)

        # Get the top N genres for each track for additional weighting
        N = 3
        base_top = sorted(base_genres.items(), key=lambda x: x[1], reverse=True)[:N]
        other_top = sorted(other_genres.items(), key=lambda x: x[1], reverse=True)[:N]

        # Calculate overlap bonus for shared primary genres
        base_top_genres = set(g[0] for g in base_top)
        other_top_genres = set(g[0] for g in other_top)
        genre_overlap = len(base_top_genres & other_top_genres)
        overlap_bonus = genre_overlap / N  # Bonus factor based on shared top genres

        # Combine cosine similarity with overlap bonus
        final_similarity = (dot_product + overlap_bonus) / 2

        return final_similarity

    def get_similar_tracks(
        self, track_path: str, n: int = 5
    ) -> List[Tuple[str, float]]:
        if track_path not in self.tracks_features:
            return []

        base_features = self.tracks_features[track_path]
        similarities = []

        # Get weights from GUI
        w_rhythm = dpg.get_value("weight_rhythm")
        w_tonal = dpg.get_value("weight_tonal")
        w_energy = dpg.get_value("weight_energy")
        w_mood = dpg.get_value("weight_mood")
        w_genre = dpg.get_value("weight_genre")
        w_timbral = dpg.get_value("weight_timbral")

        for other_path, other_features in self.tracks_features.items():
            if other_path == track_path:
                continue

            # Calculate genre similarity using the new method
            genre_sim = self.calculate_genre_similarity(
                dict(
                    zip(base_features.genre_labels, base_features.genre_probabilities)
                ),
                dict(
                    zip(other_features.genre_labels, other_features.genre_probabilities)
                ),
            )
            genre_dist = w_genre * (1 - genre_sim)  # Convert similarity to distance

            # Rest of the distance calculations remain the same
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

            energy_dist = w_energy * np.mean(
                [
                    abs(base_features.energy - other_features.energy),
                ]
            )

            mirex_dist = np.mean(
                [abs(a - b) for a, b in zip(base_features.mirex, other_features.mirex)]
            ).tolist()

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
            artist = other_features.last_modified
            if artist == base_features.last_modified:
                artist_bonus = 0.2  # Bonus for same artist
            else:
                artist_bonus = 0

            # Combine distances with artist bonus
            total_distance = (
                rhythm_dist
                + tonal_dist
                + energy_dist
                + mood_dist
                + genre_dist
                + timbral_dist
            ) / (w_rhythm + w_tonal + w_energy + w_mood + w_genre + w_timbral)

            # Apply artist bonus
            total_distance = max(0, total_distance - artist_bonus)

            similarities.append((other_path, total_distance))

        similarities.sort(key=lambda x: x[1])
        return similarities[:n]

    def on_track_selected(self, sender, app_data):
        selected_name = app_data
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

            text_val = (
                "====================\n"
                + "Basic Info\n"
                + "====================\n"
                + f"BPM: {features.bpm:.1f}\n"
                + f"Energy: {features.energy:.2f}\n"
                + "====================\n"
                + "Genre Probabilities \n"
                + "====================\n"
                + "\n".join(
                    f"{label}: {probability:.2f}"
                    for label, probability in zip(
                        features.genre_labels, features.genre_probabilities
                    )
                )
                + "\n====================\n"
                + "Mood Analysis\n"
                + "====================\n"
                + f"Sad: {features.mood_sad[0]:.2f}\n"
                + f"Relaxed: {features.mood_relaxed[0]:.2f}\n"
                + f"Aggressive: {features.mood_aggressive[0]:.2f}\n"
                + f"Danceable: {features.danceability[0]:.2f}\n"
                + f"passionate, rousing, confident, boisterous, rowdy: {features.mirex[0]:.2f}\n"
                + f"rollicking, cheerful, fun, sweet, amiable/good natured: {features.mirex[1]:.2f}\n"
                + f"literate, poignant, wistful, bittersweet, autumnal, brooding: {features.mirex[2]:.2f}\n"
                + f"humorous, silly, campy, quirky, whimsical, witty, wry: {features.mirex[3]:.2f}\n"
                + f"aggressive, fiery, tense/anxious, intense, volatile, visceral: {features.mirex[4]:.2f}\n"
                + "====================\n"
                + "Rhythm\n"
                + "====================\n"
                + f"Strength: {features.rhythm_strength:.2f}\n"
                + f"Regularity: {features.rhythm_regularity:.2f}\n"
                + "====================\n"
            )
            dpg.set_value("features_text", text_val)

            # Find and display similar tracks
            similar = self.get_similar_tracks(selected_path)
            similar_text = "Similar Tracks:\n" + "\n".join(
                f"{Path(path).name}: {dist:.3f} similarity score"
                for path, dist in similar
            )
            dpg.set_value("similar_tracks_text", similar_text)

    def save_analysis(self, filename: str):
        if not filename.endswith(".json"):
            filename += ".json"
        serialized_features = {
            path: features.to_dict() for path, features in self.tracks_features.items()
        }
        with open(filename, "w") as f:
            json.dump(serialized_features, f, indent=2)

    def load_analysis(self, filename: str):
        with open(filename, "r") as f:
            data = json.load(f)
            self.tracks_features = {
                path: AudioFeatures.from_dict(feat_dict)
                for path, feat_dict in data.items()
            }
            # Update tracks list
            track_names = [Path(p).name for p in self.tracks_features.keys()]
            dpg.configure_item("tracks_list", items=track_names)

    def run(self):
        while dpg.is_dearpygui_running():
            dpg.render_dearpygui_frame()
        dpg.destroy_context()


if __name__ == "__main__":
    app = MusicAnalyzerGUI()
    app.run()
