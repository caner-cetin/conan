from dataclasses import asdict, dataclass
from typing import Any

from pydantic import BaseModel


@dataclass
class AudioFeatures:
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
    mfcc_mean: list[float]
    mfcc_var: list[float]
    onset_rate: list[float]
    pitch: float
    loudness: float
    last_modified: str
    file_hash: str
    metadata: dict[str, str | float | list[str]]

    def to_dict(self) -> dict[str, float | str | list[float]]:
        return asdict(self)

    @classmethod
    def from_dict(cls, data: dict[str, Any]) -> "AudioFeatures":
        return cls(**data)
    
class BeefWeb:
    # https://hyperblast.org/beefweb/api/#/player/getPlayerState
    class PlayerState:
        class Player(BaseModel):
            info: 'BeefWeb.PlayerState.Info'
            activeItem: 'BeefWeb.PlayerState.ActiveItem'
            playbackState: str
            volume: 'BeefWeb.PlayerState.Volume'
        class Volume(BaseModel):
            type: str
            min: int
            max: int
            value: int
            isMuted: bool
        class ActiveItem(BaseModel):
            playlistId: str
            playlistIndex: int
            index: int
            position: int
            duration: int
            columns: list[str]
        class Info(BaseModel):
            name: str
            title: str
            version: str
            pluginVersion: str
        class State(BaseModel):
            player: 'BeefWeb.PlayerState.Player'

_ = BeefWeb.PlayerState.State.model_rebuild()