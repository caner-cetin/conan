from typing import (
    Any,
    Literal,
    Optional,
    TypeAlias,
)

import numpy as np
from numpy.typing import NDArray

class MonoLoader:
    def __init__(
        self,
        filename: str | None = None,
        sampleRate: int = 44100,
        resampleQuality: int = 4,
    ) -> None: ...
    def __call__(self) -> NDArray[np.float32]: ...
    def configure(
        self, sampleRate: float, resampleQuality: int, filename: str
    ) -> None: ...

class TensorflowPredict2D:
    def __init__(
        self,
        graphFilename: str | None = None,
        input: str = "model/Placeholder",
        output: str = "model/Sigmoid",
    ) -> None: ...
    def __call__(self, features: NDArray[np.float32]) -> NDArray[np.float32]: ... # this is a 2D array.

class TensorflowPredictEffnetDiscogs:
    def __init__(
        self,
        graphFilename: str | None = None,
        input: str = "model/Placeholder",
        output: str = "model/Sigmoid",
        patchHopSize: int | None = None
    ) -> None: ...
    def __call__(self, signal: NDArray[np.float32]) -> NDArray[np.float32]: ...

class TensorflowPredictMusiCNN:
    def __init__(
        self,
        graphFilename: str | None = None,
        input: str = "model/Placeholder",
        output: str = "model/Sigmoid",
    ) -> None: ...
    def __call__(self, signal: NDArray[np.float32]) -> NDArray[Any]: ... # pyright: ignore[reportExplicitAny] I cant type hint NDArray[NDArray[np.float32]], this is a 2D array.

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

class OnsetRate:
    def __init__(self) -> None: ...
    def __call__(self, signal: NDArray[np.float32]) -> tuple[NDArray[np.float32], float]:
        """This algorithm computes the number of onsets per second and their position in time for an audio signal. 
        
        Onset detection functions are computed using both high frequency content and complex-domain methods available in OnsetDetection algorithm. 
        See OnsetDetection for more information. 
        Please note that due to a dependence on the Onsets algorithm, this algorithm is only valid for audio signals with a sampling rate of 44100Hz. 
        This algorithm throws an exception if the input signal is empty.
        
        Args:
            signal (NDArray[np.float32]): Input signal from essentia.standard.MonoLoader

        Returns:
            tuple[NDArray[np.float32], float]: the detected onset times [s] and the number of onsets per second in order
        """
        ...

class MFCC:
    def __init__(
        self,
        dctType: int = 2,
        highFrequencyBound: float = 11000,
        inputSize: int = 1025,
        liftering: int = 0,
        logType: Literal["natural", "dbpow", "dbamp", "log"] = "dbamp",
        lowFrequencyBound: float = 0,
        normalize: Literal["unit_sum", "unit_tri", "unit_max"]= "unit_sum",
        numberBands: int = 40,
        numberCoefficients: int = 13,
        sampleRate: float = 44100,
        silenceThreshold: float = 1e-10,
        type: Literal["magnitude", "power"] = "power",
        warpingFormula: Literal["slaneyMel", "htkMel"] = "htkMel",
        weighting: Literal["warping", "linear"] = "warping"
    ) -> None: 
        """
        Args:
            dctType (int, optional): the DCT type. Defaults to 2.
            highFrequencyBound (float, optional): the upper bound of the frequency range [Hz]. Defaults to 11000.
            inputSize (int, optional): the size of input spectrum. Defaults to 1025.
            liftering (int, optional): the liftering coefficient. Use ‘0’ to bypass it. Defaults to 0.
            logType (Literal[&quot;natural&quot;, &quot;dbpow&quot;, &quot;dbamp&quot;, &quot;log&quot;], optional): logarithmic compression type. Use ‘dbpow’ if working with power and ‘dbamp’ if working with magnitudes. Defaults to "dbamp".
            lowFrequencyBound (float, optional): the lower bound of the frequency range [Hz]. Defaults to 0.
            normalize (Literal[&quot;unit_sum&quot;, &quot;unit_tri&quot;, &quot;unit_max&quot;], optional): spectrum bin weights to use for each mel band: ‘unit_max’ 
            to make each mel band vertex equal to 1, ‘unit_sum’ to make each mel band area equal to 1 summing the actual weights of spectrum bins, 
            ‘unit_area’ to make each triangle mel band area equal to 1 normalizing the weights of each triangle by its bandwidth. Defaults to "unit_sum".
            numberBands (int, optional): the number of mel-bands in the filter. Defaults to 40.
            numberCoefficients (int, optional): the number of output mel coefficients. Defaults to 13.
            sampleRate (float, optional): the sampling rate of the audio signal [Hz]. Defaults to 44100.
            silenceThreshold (float, optional): silence threshold for computing log-energy bands. Defaults to 1e-10.
            type (Literal[&quot;magnitude&quot;, &quot;power&quot;], optional): use magnitude or power spectrum. Defaults to "power".
            warpingFormula (Literal[&quot;slaneyMel&quot;, &quot;htkMel&quot;], optional): The scale implementation type: ‘htkMel’ scale from the HTK toolkit [2, 3] (default) or ‘slaneyMel’ scale from the Auditory toolbox [4]. Defaults to "htkMel".
            weighting (Literal[&quot;warping&quot;, &quot;linear&quot;], optional): type of weighting function for determining triangle area. Defaults to "warping".
        """
    def __call__(self, spectrum: NDArray[np.float32]) -> tuple[NDArray[np.float32], NDArray[np.float32]]: 
        """This algorithm computes the mel-frequency cepstrum coefficients of a spectrum. 

        Args:
            spectrum (NDArray[np.float32]): Audio spectrum

        Returns:
            tuple[NDArray[np.float32], NDArray[np.float32]]: the energies in mel bands and the mel frequency cepstrum coefficients in order
        """
        ...

class RhythmExtractor2013:
    def __init__(
        self,
        maxTempo: int = 208,
        method: Literal["multifeature", "degara"] = "multifeature",
        minTempo: int = 40
    ) -> None: 
        """

        Args:
            maxTempo (int, optional): the fastest tempo to detect [bpm] in range of 60-250. Defaults to 208.
            method (Literal[&quot;multifeature&quot;, &quot;degara&quot;], optional): the method used for beat tracking. Defaults to "multifeature".
            minTempo (int, optional): the slowest tempo to detect [bpm] in range of 40-180. Defaults to 40.
        """
    def __call__(self, signal: NDArray[np.float32]) -> tuple[float, NDArray[np.float32], float, NDArray[np.float32], NDArray[np.float32]]: 
        """This algorithm extracts the beat positions and estimates their confidence as well as tempo in bpm for an audio signal. The beat locations can be computed using:

        Args:
            signal (NDArray[np.float32]): Input signal from essentia.standard.MonoLoader

        Returns:
            tuple[float, NDArray[np.float32], float, NDArray[np.float32], NDArray[np.float32]]: 
            
            -> the tempo estimation [bpm]
            
            -> the estimated tick locations [s]
            
            -> confidence with which the ticks are detected (ignore this value if using ‘degara’ method)
            
            -> the list of bpm estimates characterizing the bpm distribution for the signal [bpm]
            
            -> list of beats interval [s]
            
            in order
        """

class PredominantPitchMelodia:
    def __init__(self) -> None: ...
    def __call__(self, signal: NDArray[np.float32]) -> tuple[NDArray[np.float32], NDArray[np.float32]]: 
        """This algorithm estimates the fundamental frequency of the predominant melody from polyphonic music signals using the MELODIA algorithm.
        
        It is specifically suited for music with a predominent melodic element, for example the singing voice melody in an accompanied singing recording. 

        Args:
            signal (NDArray[np.float32]): Input signal from essentia.standard.MonoLoader

        Returns:
            tuple[NDArray[np.float32], NDArray[np.float32]]: the estimated pitch values [Hz] and confidence with which the pitch was detected in order
        """