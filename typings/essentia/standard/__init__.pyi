from typing import (
    Any,
    Optional,
)

import numpy as np
from numpy.typing import NDArray

class MonoLoader:
    def __init__(
        self,
        filename: Optional[str] = None,
        sampleRate: Optional[int] = 44100,
        resampleQuality: Optional[int] = 4,
    ) -> None: ...
    def compute(self) -> tuple[Any]: ...
    def __call__(self, *args: Any, **kwds: Any) -> NDArray[np.float32]: ...
    def configure(
        self, sampleRate: float, resampleQuality: int, filename: str
    ) -> None: ...

class TensorflowPredict2D:
    def __init__(
        self,
        graphFilename: Optional[str] = None,
        input: Optional[str] = "model/Placeholder",
        output: Optional[str] = "model/Sigmoid",
    ) -> None: ...
    def compute(self, features: Any) -> tuple[Any]: ...
    def __call__(self, *args: Any, **kwds: Any) -> NDArray[np.float32]: ...

class TensorflowPredictEffnetDiscogs:
    def __init__(
        self,
        graphFilename: Optional[str] = None,
        input: Optional[str] = "model/Placeholder",
        output: Optional[str] = "model/Sigmoid",
        patchHopSize: Optional[int] = None
    ) -> None: ...
    def compute(self, features: Any) -> tuple[Any]: ...
    def __call__(self, *args: Any, **kwds: Any) -> NDArray[np.float32]: ...

class TensorflowPredictMusiCNN:
    def __init__(
        self,
        graphFilename: Optional[str] = None,
        input: Optional[str] = "model/Placeholder",
        output: Optional[str] = "model/Sigmoid",
    ) -> None: ...
    def compute(self, features: Any) -> tuple[Any]: ...
    def __call__(self, *args: Any, **kwds: Any) -> Any: ...

class Loudness:
    def __init__(self) -> None: ...
    def __call__(self, signal: NDArray[np.float32]) -> float:
        """This algorithm computes the loudness of an audio signal defined by Steven’s power law. It computes loudness as the energy of the signal raised to the power of 0.67.

        Args:
            signal (NDArray[np.float32]): Input signal from essentia.standard.MonoLoader

        Returns:
            float: Loudness of the input signal.
        """
        ...

class EqualLoudness:
    def __init__(self) -> None: ...
    def __call__(self, signal: NDArray[np.float32]) -> NDArray[np.float32]:
        """This algorithm implements an equal-loudness filter.

        The human ear does not perceive sounds of all frequencies as having equal loudness, and to account for this, the signal is filtered by an inverted approximation of the equal-loudness curves.
        Technically, the filter is a cascade of a 10th order Yulewalk filter with a 2nd order Butterworth high pass filter.

        This algorithm depends on the IIR algorithm. Any requirements of the IIR algorithm are imposed for this algorithm.
        This algorithm is only defined for the sampling rates specified in parameters. It will throw an exception if attempting to configure with any other sampling rate.

        Args:
            signal (NDArray[np.float32]): Input signal from essentia.standard.MonoLoader

        Returns:
            NDArray[np.float32]: Output signal after equal loudness filtering.
        """
        ...

class DCRemoval:
    def __init__(self) -> None: ...
    def __call__(self, signal: NDArray[np.float32]) -> NDArray[np.float32]: ...

class BandPass:
    def __init__(
        self,
        bandwidth: float = 500,
        cutoffFrequency: float = 1500,
        sampleRate: float = 44100,
    ) -> None:
        """

        Args:
            bandwidth (float, optional): the bandwidth of the filter [Hz]. Defaults to 500.
            cutoffFrequency (float, optional): the cutoff frequency for the filter [Hz]. Defaults to 1500.
            sampleRate (float, optional): the sampling rate of the audio signal [Hz]. Defaults to 44100.
        """
    def __call__(self, signal: NDArray[np.float32]) -> NDArray[np.float32]: ...

class MovingAverage:
    def __init__(self) -> None: ...
    def __call__(self, signal: NDArray[np.float32]) -> NDArray[np.float32]: ...
