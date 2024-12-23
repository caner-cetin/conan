import json
import os
import urllib.parse
from concurrent.futures import ThreadPoolExecutor
from typing import Any, final
from urllib.parse import quote

import urllib3
from essentia import standard
from loguru import logger
from PySide6.QtCore import QObject, QSize, QTimer, Signal, Slot
from PySide6.QtGui import QColor, QMovie, QPainter, QPixmap, Qt
from PySide6.QtWebEngineCore import QWebEngineSettings
from PySide6.QtWebEngineWidgets import QWebEngineView
from PySide6.QtWidgets import (
    QHBoxLayout,
    QLabel,
    QMainWindow,
    QStyle,
    QTextEdit,
    QVBoxLayout,
)
from urllib3.poolmanager import PoolManager

from constants import ActiveTrack, BeefWeb
from templates import TrackPlayerInfoTemplate


@final
class TrackInfo(QWebEngineView):
    def __init__(self):
        super().__init__()
        self.track_info_page = self.page()
        self.track_info_page.setBackgroundColor(QColor(0, 0, 0, 0))
        self.track_info_settings = self.track_info_page.settings()
        self.track_info_settings.setAttribute(
            QWebEngineSettings.WebAttribute.JavascriptEnabled, True
        )
        self.track_info_settings.setAttribute(
            QWebEngineSettings.WebAttribute.LocalContentCanAccessRemoteUrls, True
        )


# https://editor.swagger.io/
# import from url https://raw.githubusercontent.com/hyperblast/beefweb/96091e9e15a32211e499f447e9112d334311bcb3/docs/player-api.yml
@final
class Foobar2K(QObject):
    api_url: str | None
    __state: BeefWeb.PlayerState.State | None = None
    __state_changed = Signal(object)
    __active_track_changed = Signal(bool)
    __rerender_player_info = Signal(bool)
    __active_track: ActiveTrack | None = None
    __poll_timer: QTimer | None = None
    player_layout: QHBoxLayout

    def __init__(self, parent: QMainWindow) -> None:
        self.api_url = os.getenv("API_URL")
        if self.api_url is None:
            raise Exception("Api url for BeefWeb is not set in .env")

        super().__init__(parent=parent)
        self.__http = PoolManager(
            maxsize=4, retries=urllib3.Retry(3, backoff_factor=0.1)
        )
        self.__thread_pool = ThreadPoolExecutor(max_workers=2)

        self.__setup_ui()

        self.__poll_timer = QTimer(self)
        self.__poll_timer.timeout.connect(self.__update_state)  # pyright: ignore[reportAny]
        self.__poll_timer.start(1000)

        self.__active_track_changed.connect(self.__fetch_and_update_artwork)  # pyright: ignore[reportAny]
        self.__active_track_changed.connect(self.__get_active_track_info)  # pyright: ignore[reportAny]
        self.__rerender_player_info.connect(self.__render_player_info_html)  # pyright: ignore[reportAny]

        self.__track_player_info_template = TrackPlayerInfoTemplate()

    def __setup_ui(self):
        self.cover_art: bytes | None = None
        self.cover_art_scale = QSize(120, 120)

        self.cover_art_container = QLabel()
        self.cover_art_container.setFixedSize(self.cover_art_scale)
        self.cover_art_container.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.cover_art_container.setScaledContents(False)

        self.cover_art_placeholder = QMovie("./assets/no_cover_art.gif")
        self.cover_art_placeholder.setScaledSize(self.cover_art_scale)

        self.track_player_info_container = QTextEdit()
        self.track_player_info_container.setMaximumHeight(120)
        self.track_player_info_container.setReadOnly(True)

        self.player_layout = QHBoxLayout()
        self.player_layout.addWidget(self.cover_art_container)
        self.player_layout.addWidget(self.track_player_info_container)

        self.__show_placeholder()

    @Slot()  # pyright: ignore[reportAny]
    def cleanup(self):
        """Cleanup method to be called before application exit"""
        if hasattr(self, "__poll_timer"):
            if self.__poll_timer is not None:
                self.__poll_timer.stop()
        if hasattr(self, "__thread_pool"):
            self.__thread_pool.shutdown(wait=True)

    def __make_request(
        self, method: str, url: str
    ) -> tuple[bool, dict[str, Any] | bytes | None, urllib3.BaseHTTPResponse]:
        response = self.__http.request(method, url, timeout=2.0)
        if response.status != 200:
            return False, None, response

        if "application/json" in response.headers.get("Content-Type", ""):
            return True, json.loads(response.data.decode("utf-8")), response
        return True, response.data, response

    @Slot()  # pyright: ignore[reportAny]
    def play(self):
        try:
            success, _, _ = self.__make_request("POST", f"{self.api_url}/player/play")
            if not success:
                print("Failed to send play command")
        except Exception as e:
            print(f"Error in play command: {e}")

    @Slot()  # pyright: ignore[reportAny]
    def __update_state(self):
        success, data, _ = self.__make_request("GET", f"{self.api_url}/player")
        if not success or not data:
            return
        if isinstance(data, dict):
            st = BeefWeb.PlayerState.State(**data)  # pyright: ignore[reportAny]
            old_st = self.__state
            self.__state = st
            if old_st is not None:
                playlist_id_changed = (
                    st.player.activeItem.playlistId
                    != old_st.player.activeItem.playlistId
                )
                active_index_changed = (
                    st.player.activeItem.index != old_st.player.activeItem.index
                )
                if playlist_id_changed or active_index_changed:
                    self.__active_track_changed.emit(True)
                playback_state_changed = (
                    st.player.playbackState != old_st.player.playbackState
                )
                if playback_state_changed:
                    self.__rerender_player_info.emit(True)

            else:
                self.__active_track_changed.emit(True)
        else:
            self.__state = None

    @Slot()  # pyright: ignore[reportAny]
    def __fetch_and_update_artwork(self) -> None:
        if self.__state is None:
            return

        url = (
            f"{self.api_url}/artwork/"
            f"{self.__state.player.activeItem.playlistId}/"
            f"{self.__state.player.activeItem.index}"
        )

        success, data, _ = self.__make_request("GET", url)
        if success and data and isinstance(data, bytes):
            self.cover_art = data
            QTimer.singleShot(0, self.__update_artwork)
        else:
            QTimer.singleShot(0, self.__show_placeholder)

    def __show_placeholder(self):
        self.cover_art_placeholder.stop()
        self.cover_art_placeholder.setScaledSize(self.cover_art_scale)
        self.cover_art_container.setMovie(self.cover_art_placeholder)
        self.cover_art_placeholder.start()

    def __update_artwork(self):
        try:
            if not hasattr(self, "cover_art") or self.cover_art is None:
                self.__show_placeholder()
                return

            pixmap = QPixmap()
            pixmap.loadFromData(self.cover_art)
            scaled_pixmap = pixmap.scaled(
                self.cover_art_scale,
                Qt.AspectRatioMode.KeepAspectRatio,
                Qt.TransformationMode.SmoothTransformation,
            )

            x_offset = (self.cover_art_scale.width() - scaled_pixmap.width()) // 2
            y_offset = (self.cover_art_scale.height() - scaled_pixmap.height()) // 2

            container_pixmap = QPixmap(self.cover_art_scale)
            container_pixmap.fill(Qt.GlobalColor.transparent)

            painter = QPainter(container_pixmap)
            painter.drawPixmap(x_offset, y_offset, scaled_pixmap)
            painter.end()

            if self.cover_art_container.movie():
                self.cover_art_container.movie().stop()
                self.cover_art_container.clear()
            self.cover_art_container.setPixmap(container_pixmap)

        except Exception as e:
            print(f"Error updating artwork: {e}")
            self.__show_placeholder()

    @Slot()  # pyright: ignore[reportAny]
    def __get_active_track_info(self):
        if self.__state is None:
            return
        valid_playlist_id = (
            self.__state.player.activeItem.playlistId != ""
            and self.__state.player.activeItem.playlistId is not None
        )
        if valid_playlist_id:
            # all available columns are listed under doc/titleformat_help.html
            columns = [
                "%artist%]",
                "%title%]",
                "%album%]",
                "%length%]",
                "%tracknumber%]",
                "%totaltracks%]",
            ]
            columns_param = quote(json.dumps(columns))

            success, data, response = self.__make_request(
                "GET",
                (
                    f"{self.api_url}/playlists/"
                    f"{self.__state.player.activeItem.playlistId}/items/"
                    f"0:{self.__state.player.activeItem.index+1}?"
                    f"columns={columns_param}"
                ),
            )
            if not success:
                logger.error(
                    f"<< {response.url} {response.status} {response.data} {response.json()}"
                )
                return

            if isinstance(data, dict):
                playlist = BeefWeb.PlaylistItems.Playlist(**data)
                cols = playlist.playlistItems.items[
                    self.__state.player.activeItem.index
                ].columns
                for i, col in enumerate(cols):
                    cols[i] = col.strip().replace('"', "", -1)
                self.__active_track = ActiveTrack(
                    artist=cols[0],
                    title=cols[1],
                    album=cols[2],
                    length=cols[3],
                    track_number=int(cols[4]),
                    total_tracks=int(cols[5]) if cols[5] != "?" else 0,
                )
                self.__rerender_player_info.emit(True)

    @Slot()  # pyright: ignore[reportAny]
    def __render_player_info_html(self):
        if self.__active_track is not None and self.__state is not None:
            t = self.__track_player_info_template.update_display(
                title=f"{self.__active_track.artist} - {self.__active_track.title}",
                playback_status=self.__state.player.playbackState,
            )
            self.track_player_info_container.setHtml(t)
        else:
            t = self.__track_player_info_template.update_display(
                title="zzz...",
                playback_status="stopped",
            )
            self.track_player_info_container.setHtml(t)
