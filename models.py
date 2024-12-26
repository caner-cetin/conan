import hashlib
from datetime import datetime
from pathlib import Path
from typing import final

import numpy as np
from essentia.standard import (
    MFCC,
    BandPass,
    DCRemoval,
    EqualLoudness,
    Loudness,
    MonoLoader,
    MovingAverage,
    OnsetRate,
    PredominantPitchMelodia,
    RhythmExtractor2013,
    TensorflowPredict2D,
    TensorflowPredictEffnetDiscogs,
    TensorflowPredictMusiCNN,
)
from numpy.typing import NDArray
from tinytag import TinyTag

from constants import AudioFeatures, AudioMetadata
from labels import genres


def process_labels(label: str):
    _, style = label.split("---")
    return f"{style}"


processed_genres = list(map(process_labels, genres))


@final
class Analyzer:
    def __init__(self):
        self.effnet_model = TensorflowPredictEffnetDiscogs(
            graphFilename="./models/discogs-effnet-bs64-1.pb",
            output="PartitionedCall:1",
        )

        self.musicnn_model = TensorflowPredictMusiCNN(
            graphFilename="./models/msd-musicnn-1.pb",
            output="model/dense/BiasAdd",
        )

        self.genre_model = TensorflowPredict2D(
            graphFilename="./models/genre_discogs400-discogs-effnet-1.pb",
            input="serving_default_model_Placeholder",
            output="PartitionedCall:0",
        )

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

        self.mirex_model = TensorflowPredict2D(
            graphFilename="./models/moods_mirex-msd-musicnn-1.pb",
            input="serving_default_model_Placeholder",
            output="PartitionedCall:0",
        )

        self.instrumental_or_vocal_model = TensorflowPredict2D(
            graphFilename="./models/voice_instrumental-discogs-effnet-1.pb",
            output="model/Softmax",
        )

        self.vocal_gender_model = TensorflowPredict2D(
            graphFilename="./models/gender-discogs-effnet-1.pb", output="model/Softmax"
        )

        self.engaging_model = TensorflowPredict2D(
            graphFilename="./models/engagement_3c-discogs-effnet-1.pb",
            output="model/Softmax",
        )
        self.rhythm_extractor = RhythmExtractor2013()
        self.pitch_extractor = PredominantPitchMelodia()
        self.loudness = Loudness()
        self.mfcc = MFCC()
        self.onset_rate = OnsetRate()
        self.dc_remover = DCRemoval()
        self.equalizer = EqualLoudness()
        self.bandpass = BandPass(
            cutoffFrequency=2000,
            bandwidth=19980,
            sampleRate=44100,
        )
        self.moving_average = MovingAverage()
        self.loader = MonoLoader()

    def genre(
        self, effnet: NDArray[np.float32], n: int = 10
    ) -> tuple[list[str], list[float]]:
        """Returns probability of each genre for the given audio signal.

        There are 400 genres available in the Discogs classifier and you can read them all in
        metadata file (genre_discogs400-discogs-effnet-1.json) inside the models folder.

        Args:
            audio (NDArray[np.float32]): Output of essentia.standard.TensorflowPredictEffnetDiscogs, called with audio signal from essentia.standard.MonoLoader.
            n: Top N genres to return

        Returns:
            tuple[list[Any], list[Any]]: Two lists containing the top N genre labels and their probabilities in order.
        """
        acts: NDArray[np.float32] = np.mean(
            self.genre_model(effnet),
            axis=0,
        )

        top_n_idx = np.argsort(acts)[::-1][:n]
        return [processed_genres[idx] for idx in top_n_idx], [
            float(acts[idx]) for idx in top_n_idx
        ]

    def aggressive(self, effnet: NDArray[np.float32]) -> list[float]:
        """Returns the probability of the audio being aggressive.

        Args:
            audio (NDArray[np.float32]): Output of essentia.standard.TensorflowPredictEffnetDiscogs, called with audio signal from essentia.standard.MonoLoader.

        Returns:
            list[float]: Two element list containing the probability of the audio being aggressive and not aggressive in order.
        """
        return np.mean(
            self.aggressive_model(effnet),
            axis=0,
        ).tolist()

    def sad(self, effnet: NDArray[np.float32]) -> list[float]:
        """Returns the probability of the audio being sad.

        Args:
            audio (NDArray[np.float32]): Output of essentia.standard.TensorflowPredictEffnetDiscogs, called with audio signal from essentia.standard.MonoLoader.

        Returns:
            list[float]: Two element list containing the probability of the audio being sad and not sad in order.
        """
        return np.mean(
            self.sad_model(effnet),
            axis=0,
        ).tolist()

    def relaxed(self, effnet: NDArray[np.float32]) -> list[float]:
        """Returns the probability of the audio being relaxed.

        Args:
            audio (NDArray[np.float32]): Output of essentia.standard.TensorflowPredictEffnetDiscogs, called with audio signal from essentia.standard.MonoLoader.

        Returns:
            list[float]: Two element list containing the probability of the audio being relaxed and not relaxed in order.
        """
        return np.mean(
            self.relaxed_model(effnet),
            axis=0,
        ).tolist()

    def danceability(self, effnet: NDArray[np.float32]) -> list[float]:
        """Returns the probability of the audio being danceable.

        Args:
            audio (NDArray[np.float32]): Output of essentia.standard.TensorflowPredictEffnetDiscogs, called with audio signal from essentia.standard.MonoLoader.

        Returns:
            list[float]: Two element list containing the probability of the audio being danceable and not danceable in order.
        """
        return np.mean(
            self.danceability_model(effnet),
            axis=0,
        ).tolist()

    def mirex(self, musicnn: NDArray[np.float32]) -> list[float]:
        """Music classification by mood with the MIREX Audio Mood Classification Dataset (5 mood clusters)

        1. passionate, rousing, confident, boisterous, rowdy
        2. rollicking, cheerful, fun, sweet, amiable/good natured
        3. literate, poignant, wistful, bittersweet, autumnal, brooding
        4. humorous, silly, campy, quirky, whimsical, witty, wry
        5. aggressive, fiery, tense/anxious, intense, volatile, visceral

        Args:
            audio (NDArray[np.float32]): Output of essentia.standard.TensorflowPredictMusiCNN, called with audio signal from essentia.standard.MonoLoader.

        Returns:
            list[float]: Five element list containing the probability of the audio being in each mood cluster.
        """
        return np.mean(
            self.mirex_model(musicnn),
            axis=0,
        ).tolist()

    def instrumental_or_vocal(self, effnet: NDArray[np.float32]) -> list[float]:
        """Classification of music by presence or absence of voice

        Args:
            audio (NDArray[np.float32]): Output of essentia.standard.TensorflowPredictEffnetDiscogs, called with audio signal from essentia.standard.MonoLoader.

        Returns:
            list[float]: Two element list containing the probability of the audio being instrumental or voice in order.
        """
        return np.mean(self.instrumental_or_vocal_model(effnet), axis=0).tolist()

    def vocal_gender(self, effnet: NDArray[np.float32]) -> list[float]:
        """Classification of music by singing voice gender

        Args:
            audio (NDArray[np.float32]): Output of essentia.standard.TensorflowPredictEffnetDiscogs, called with audio signal from essentia.standard.MonoLoader.

        Returns:
            list[float]: Two element list containing the probability of the vocal being female or male in order.
        """
        return np.mean(self.vocal_gender_model(effnet), axis=0).tolist()

    def engaging(self, effnet: NDArray[np.float32]) -> list[float]:
        """Music engagement predicts whether the music evokes active attention of the listener (high-engagement “lean forward” active listening vs. low-engagement “lean back” background listening). Outputs three levels of engagement.

        Args:
            audio (NDArray[np.float32]): Output of essentia.standard.TensorflowPredictEffnetDiscogs, called with audio signal from essentia.standard.MonoLoader.

        Returns:
            list[float]: Three element list containing the probability of the audio being not engaging, moderately engaging or engaging in order.
        """
        return np.mean(self.engaging_model(effnet), axis=0).tolist()

    def analyze_track(self, audio_path: Path) -> AudioFeatures:
        self.loader.configure(
            sampleRate=16000,
            resampleQuality=4,
            filename=str(audio_path),
        )
        audio: NDArray[np.float32] = self.loader()
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

        pitch, _ = self.pitch_extractor(audio)
        bpm, beats, beats_conf, _, _ = self.rhythm_extractor(audio)
        _, mfcc_coeffs = self.mfcc(audio)
        onset_rate, _ = self.onset_rate(audio)

        rhythm_strength = np.mean(beats_conf)

        effnet = self.effnet_model(audio)
        musicnn = self.musicnn_model(audio)

        tag: TinyTag = TinyTag.get(audio_path)
        genre_labels, genre_probabilities = self.genre(effnet)
        if tag.genre is not None:
            genre_labels.insert(0, tag.genre)
            genre_probabilities.insert(0, 1)
        return AudioFeatures(
            bpm=float(bpm),
            rhythm_strength=float(rhythm_strength),
            rhythm_regularity=float(np.std(np.diff(beats))),
            danceability=self.danceability(effnet),
            mood_sad=self.sad(effnet),
            mood_relaxed=self.relaxed(effnet),
            mood_aggressive=self.aggressive(effnet),
            last_modified=datetime.fromtimestamp(
                audio_path.stat().st_mtime
            ).isoformat(),
            file_hash=self._get_file_hash(audio_path),
            genre_probabilities=genre_probabilities,
            genre_labels=genre_labels,
            loudness=self.loudness(audio),
            pitch=np.mean(pitch).item(),
            mirex=self.mirex(musicnn),
            mfcc_mean=np.mean(mfcc_coeffs, axis=0).tolist(),
            mfcc_var=np.var(mfcc_coeffs, axis=0).tolist(),
            onset_rate=onset_rate.tolist(),
            metadata=AudioMetadata.from_tag(tag),
            instrumental=self.instrumental_or_vocal(effnet),
            engagement=self.engaging(effnet),
            vocal_gender=self.vocal_gender(effnet),
        )

    def _get_file_hash(self, file_path: Path) -> str:
        hasher = hashlib.md5()
        with open(file_path, "rb") as f:
            buf = f.read(1024 * 1024)
            hasher.update(buf)
        return hasher.hexdigest()
