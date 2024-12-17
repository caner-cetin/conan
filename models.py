from typing import Any

import numpy as np
from essentia.standard import (
    TensorflowPredict2D,
    TensorflowPredictEffnetDiscogs,
    TensorflowPredictMusiCNN,
)
from numpy.typing import NDArray

from labels import genres


def process_labels(label):
    _, style = label.split("---")
    return f"{style}"


processed_genres = list(map(process_labels, genres))


class Classifier:
    def __init__(self):
        # Initialize EffNet model for reuse
        self.effnet_model = TensorflowPredictEffnetDiscogs(
            graphFilename="./models/discogs-effnet-bs64-1.pb",
            output="PartitionedCall:1",
        )

        # Initialize genre classifier
        self.genre_model = TensorflowPredict2D(
            graphFilename="./models/genre_discogs400-discogs-effnet-1.pb",
            input="serving_default_model_Placeholder",
            output="PartitionedCall:0",
        )

        # Initialize mood classifiers
        self.aggressive_model = TensorflowPredict2D(
            graphFilename="./models/mood_aggressive-discogs-effnet-1.pb",
            output="model/Softmax",
        )

        self.sad_model = TensorflowPredict2D(
            graphFilename="./models/mood_sad-discogs-effnet-1.pb",
            output="model/Softmax",
        )

        self.relaxed_model = TensorflowPredict2D(
            graphFilename="./models/mood_relaxed-discogs-effnet-1.pb",
            output="model/Softmax",
        )

        self.danceability_model = TensorflowPredict2D(
            graphFilename="./models/danceability-discogs-effnet-1.pb",
            output="model/Softmax",
        )

        # Initialize MIREX models
        self.musicnn_model = TensorflowPredictMusiCNN(
            graphFilename="./models/msd-musicnn-1.pb",
            output="model/dense/BiasAdd",
        )

        self.mirex_model = TensorflowPredict2D(
            graphFilename="./models/moods_mirex-msd-musicnn-1.pb",
            input="serving_default_model_Placeholder",
            output="PartitionedCall:0",
        )

    def genre(
        self, audio: NDArray[np.float32], n: int = 10
    ) -> tuple[list[Any], list[Any]]:
        """Returns probability of each genre for the given audio signal.

        There are 400 genres available in the Discogs classifier and you can read them all in
        metadata file (genre_discogs400-discogs-effnet-1.json) inside the models folder.

        Args:
            audio (NDArray[np.float32]): Mono audio signal from essentia.standard.MonoLoader
            n: Top N genres to return

        Returns:
            tuple[list[Any], list[Any]]: Two lists containing the top N genre labels and their probabilities in order.
        """
        effnet_output = self.effnet_model(audio)
        acts: NDArray[np.float32] = np.mean(
            self.genre_model(effnet_output),
            axis=0,
        )

        top_n_idx = np.argsort(acts)[::-1][:n]
        return [processed_genres[idx] for idx in top_n_idx], [
            float(acts[idx]) for idx in top_n_idx
        ]

    def aggressive(self, audio: NDArray[np.float32]) -> list[float]:
        """Returns the probability of the audio being aggressive.

        Args:
            audio (NDArray[np.float32]): Mono audio signal from essentia.standard.MonoLoader

        Returns:
            list[float]: Two element list containing the probability of the audio being aggressive and not aggressive in order.
        """
        effnet_output = self.effnet_model(audio)
        return np.mean(
            self.aggressive_model(effnet_output),
            axis=0,
        ).tolist()

    def sad(self, audio: NDArray[np.float32]) -> list[float]:
        """Returns the probability of the audio being sad.

        Args:
            audio (NDArray[np.float32]): Mono audio signal from essentia.standard.MonoLoader

        Returns:
            list[float]: Two element list containing the probability of the audio being sad and not sad in order.
        """
        effnet_output = self.effnet_model(audio)
        return np.mean(
            self.sad_model(effnet_output),
            axis=0,
        ).tolist()

    def relaxed(self, audio: NDArray[np.float32]) -> list[float]:
        """Returns the probability of the audio being relaxed.

        Args:
            audio (NDArray[np.float32]): Mono audio signal from essentia.standard.MonoLoader

        Returns:
            list[float]: Two element list containing the probability of the audio being relaxed and not relaxed in order.
        """
        effnet_output = self.effnet_model(audio)
        return np.mean(
            self.relaxed_model(effnet_output),
            axis=0,
        ).tolist()

    def danceability(self, audio: NDArray[np.float32]) -> list[float]:
        """Returns the probability of the audio being danceable.

        Args:
            audio (NDArray[np.float32]): Mono audio signal from essentia.standard.MonoLoader

        Returns:
            list[float]: Two element list containing the probability of the audio being danceable and not danceable in order.
        """
        effnet_output = self.effnet_model(audio)
        return np.mean(
            self.danceability_model(effnet_output),
            axis=0,
        ).tolist()

    def mirex(self, audio: NDArray[np.float32]) -> list[float]:
        """Music classification by mood with the MIREX Audio Mood Classification Dataset (5 mood clusters)

        1. passionate, rousing, confident, boisterous, rowdy
        2. rollicking, cheerful, fun, sweet, amiable/good natured
        3. literate, poignant, wistful, bittersweet, autumnal, brooding
        4. humorous, silly, campy, quirky, whimsical, witty, wry
        5. aggressive, fiery, tense/anxious, intense, volatile, visceral

        Args:
            audio (NDArray[np.float32]): Mono audio signal from essentia.standard.MonoLoader

        Returns:
            list[float]: Five element list containing the probability of the audio being in each mood cluster.
        """
        musicnn_output = self.musicnn_model(audio)
        return np.mean(
            self.mirex_model(musicnn_output),
            axis=0,
        ).tolist()
