from dataclasses import asdict, dataclass
from typing import Any, Literal

from pydantic import BaseModel


@dataclass
class AudioFeatures:
    bpm: float
    rhythm_strength: float
    rhythm_regularity: float
    danceability: list[float]
    mood_sad: list[float]
    mood_relaxed: list[float]
    mood_aggressive: list[float]
    mirex: list[float]
    genre_probabilities: list[float]
    genre_labels: list[str]
    mfcc_mean: list[float]
    mfcc_var: list[float]
    onset_rate: list[float]
    instrumental: list[float]
    engagement: list[float]
    vocal_gender: list[float]
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
    # /player
    class PlayerState:
        class Player(BaseModel):
            info: "BeefWeb.PlayerState.Info"
            activeItem: "BeefWeb.PlayerState.ActiveItem"
            playbackState: Literal["stopped", "playing", "paused"]
            volume: "BeefWeb.PlayerState.Volume"

        class Volume(BaseModel):
            type: str
            min: float
            max: float
            value: float
            isMuted: bool

        class ActiveItem(BaseModel):
            playlistId: str | None
            playlistIndex: int
            index: int
            position: float
            duration: float
            columns: list[str]

        class Info(BaseModel):
            name: str
            title: str
            version: str
            pluginVersion: str

        class State(BaseModel):
            player: "BeefWeb.PlayerState.Player"

    # /playlists/{playlistId}/items/{range}
    class PlaylistItems:
        class Playlist(BaseModel):
            playlistItems: "BeefWeb.PlaylistItems.Items"

        class Items(BaseModel):
            items: list["BeefWeb.PlaylistItems.Item"]
            offset: int
            totalCount: int

        class Item(BaseModel):
            columns: list[str]


@dataclass
class ActiveTrack:
    # Name of the artist of the track. Checks following metadata fields, in this order: “artist”, “album artist”, “composer”, “performer”.
    artist: str
    # Title of the track. If “title” metadata field is missing, file name is used instead.
    title: str
    # Name of the album specified track belongs to. Checks following metadata fields, in this order: “album”, “venue”.
    album: str
    # Two-digit index of specified track within the album. Available only when “tracknumber” field is present in track’s metadata.
    track_number: int
    # Two-digit number of tracks within the album specified track belongs to. Available only when “totaltracks” field is present in track’s metadata.
    total_tracks: int
    # Length of the track, formatted as [HH:]MM:SS.
    length: str


_ = BeefWeb.PlayerState.State.model_rebuild()
