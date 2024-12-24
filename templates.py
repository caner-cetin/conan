import json
from string import Template
from typing import final

import numpy as np

from constants import AudioFeatures


@final
class TrackDisplayTemplate:
    def __init__(self):
        with open("track_template.html", "r") as file:
            self.template = Template(file.read())

    def update_display(
        self,
        features: AudioFeatures,
        similar_tracks: list[tuple[str, float, AudioFeatures]],
        label: str,
    ) -> str:
        # Convert genre data to JSON strings for JavaScript
        genre_labels = json.dumps(
            [
                label
                for label, _ in zip(features.genre_labels, features.genre_probabilities)
            ]
        )
        genre_probabilities = json.dumps(
            [
                prob
                for _, prob in zip(features.genre_labels, features.genre_probabilities)
            ]
        )

        # Prepare template variables
        template_vars = {
            "bpm": f"{features.bpm:.1f}",
            "rhythm_strength": f"{features.rhythm_strength:.2f}",
            "rhythm_regularity": f"{features.rhythm_regularity:.2f}",
            "mirex_passionate": features.mirex[0],
            "mirex_cheerful": features.mirex[1],
            "mirex_brooding": features.mirex[2],
            "mirex_humorous": features.mirex[3],
            "mirex_aggressive": features.mirex[4],
            "sad": features.mood_sad[0],
            "relaxed": features.mood_relaxed[0],
            "aggressive": features.mood_aggressive[0],
            "danceable": features.danceability[0],
            "genre_labels": genre_labels,
            "genre_probabilities": genre_probabilities,
            "metadata": self.__format_metadata(features.metadata),
            "metadata_title": label,
            "pitch": np.round(features.pitch),
            "mfcc_mean": f"{features.mfcc_mean:.2f}",
            "mfcc_var": f"{features.mfcc_var:.2f}",
            "onset_rates": features.onset_rate,
            "instrumental": features.instrumental,
            "vocal_gender": features.vocal_gender,
            "engagement": features.engagement,
        }

        return self.template.substitute(template_vars)

    def __format_metadata(self, metadata: dict[str, str | float | list[str]]) -> str:
        infos: list[str] = []
        ###########
        infos.append(f"""
            <div class="info-item">
                <span class="label">track</span>
                <span class="value">{metadata['track']}/{metadata.get("track_total", "?")}</span> 
            </div>              
        """)
        ############
        infos.append(f"""
            <div class="info-item">
                <span class="label">duration</span>
                <span class="value">{metadata['duration']:.2f}s</span> 
            </div>        
        """)
        ###########
        an: str
        if isinstance(metadata["album"], list):
            an = metadata["album"][0]
        elif isinstance(metadata["album"], str):
            an = metadata["album"]
        else:
            an = "unknown"
        infos.append(f"""
            <div class="info-item">
                <span class="label">album</span>
                <span class="value">{an}s</span> 
            </div> 
        """)
        ###########
        infos.append(f"""
            <div class="info-item">
                <span class="label">bitrate</span>
                <span class="value">{metadata["bitrate"]:.0f} kbps</span> 
            </div> 
        """)
        #############
        fs: float = 0
        if isinstance(metadata["filesize"], (float, int)):
            fs = metadata["filesize"] * (10**-6)
        infos.append(f"""
            <div class="info-item">
                <span class="label">filesize</span>
                <span class="value">{fs:.2f} MB</span> 
            </div>              
        """)
        return "\n".join(infos)



@final
class TrackPlayerInfoTemplate:
    template: str

    def __init__(self) -> None:
        with open("track_player_info.html", "r") as file:
            self.template = file.read()
