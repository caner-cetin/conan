import json
import os
from typing import Any, final
from urllib.parse import quote

from gevent import monkey

monkey.patch_all()
import gevent
import urllib3
from gevent.pool import Group
from loguru import logger
from PySide6.QtCore import QObject, QSize, QTimer, QUrl, Signal, Slot
from PySide6.QtGui import QColor, QIcon, QMovie, QPainter, QPixmap, Qt
from PySide6.QtWebEngineCore import QWebEngineSettings
from PySide6.QtWebEngineWidgets import QWebEngineView
from PySide6.QtWidgets import (
    QButtonGroup,
    QHBoxLayout,
    QLabel,
    QMainWindow,
    QPushButton,
    QVBoxLayout,
)
from urllib3.poolmanager import PoolManager

from backend import sse_queue
from constants import ActiveTrack, BeefWeb


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



@final
class Foobar2K(QObject):
    api_url: str | None
    __state: BeefWeb.PlayerState.State | None = None
    __active_track_changed = Signal(bool)
    __playback_state_changed = Signal(bool)
    __active_track: ActiveTrack | None = None
    __player_state_poll_timer: QTimer | None = None
    player_layout: QVBoxLayout

    def __init__(self, parent: QMainWindow) -> None:
        self.api_url = os.getenv("API_URL")
        if self.api_url is None:
            raise Exception("Api url for BeefWeb is not set in .env")

        super().__init__(parent=parent)
        self.__http = PoolManager(
            maxsize=4, retries=urllib3.Retry(3, backoff_factor=0.1)
        )
        self.__setup_ui()

        self.__player_state_poll_timer = QTimer(self)
        self.__player_state_poll_timer.timeout.connect(self.__spawn_update_state)
        self.__player_state_poll_timer.start(250)

        self.__greenlet_timer = QTimer(self)
        self.__greenlet_timer.timeout.connect(self.__reschedule_greenlets)
        self.__greenlet_timer.start()

        self.__active_track_changed.connect(self.__spawn_fetch_and_update_artwork)
        self.__active_track_changed.connect(self.__spawn_get_active_track_info)
        self.__active_track_changed.connect(self.__stream_active_track_info)
        self.__playback_state_changed.connect(self.__handle_playback_state_changed)

        self.__thread_pool = Group()

    def __setup_ui(self):
        self.cover_art: bytes | None = None
        self.cover_art_scale = QSize(120, 120)

        self.player_info_layout_lhs = QVBoxLayout()

        self.cover_art_container = QLabel()
        self.cover_art_container.setFixedSize(self.cover_art_scale)
        self.cover_art_container.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.cover_art_container.setScaledContents(False)

        self.playback_controls_layout = QHBoxLayout()
        self.playback_controls = QButtonGroup()
        self.play_pause_button = QPushButton()
        self.play_pause_button.clicked.connect(self.__spawn_play_or_pause)
        self.stop_button = QPushButton()
        self.stop_button.clicked.connect(self.__spawn_stop)

        self.playback_control_icon_size = QSize(24, 24)
        self.pause_icon = QIcon("./assets/icons/pause.svg")
        self.play_icon = QIcon("./assets/icons/play.svg")
        self.stop_icon = QIcon("./assets/icons/stop.svg")
        self.play_pause_button.setIcon(self.pause_icon)
        self.play_pause_button.setIconSize(self.playback_control_icon_size)
        self.stop_button.setIcon(self.stop_icon)
        self.stop_button.setIconSize(self.playback_control_icon_size)

        self.playback_controls.addButton(self.play_pause_button)
        self.playback_controls.addButton(self.stop_button)
        self.playback_controls_layout.addWidget(self.play_pause_button)
        self.playback_controls_layout.addWidget(self.stop_button)

        self.player_info_layout_lhs.addWidget(self.cover_art_container)
        self.player_info_layout_lhs.addLayout(self.playback_controls_layout)

        self.cover_art_placeholder = QMovie("./assets/no_cover_art.gif")
        self.cover_art_placeholder.setScaledSize(self.cover_art_scale)

        self.player_info_layout_rhs = QVBoxLayout()

        self.track_player_info_container = QWebEngineView()
        self.track_player_info_container.setMaximumHeight(144)
        self.track_player_info_container_page = self.track_player_info_container.page()
        self.track_player_info_container_page.setBackgroundColor(QColor(0, 0, 0, 0))
        self.track_player_info_container_page_settings = (
            self.track_player_info_container_page.settings()
        )
        self.track_player_info_container_page_settings.setAttribute(
            QWebEngineSettings.WebAttribute.JavascriptEnabled, True
        )
        self.track_player_info_container_page_settings.setAttribute(
            QWebEngineSettings.WebAttribute.LocalContentCanAccessRemoteUrls, True
        )
        # the reason why we load the HTML from Flask rather than from container itself directly is pretty simple:
        # we are using HTMX in track player info template
        # any page opened by web engine view or any html loaded inside the web engine view is not served by a server
        # they are all files
        # files cannot have origin
        # http requests cannot work without an origin
        # so we are just pointing to the flask server instead
        self.track_player_info_container.load(QUrl("http://127.0.0.1:31311"))

        self.player_info_layout_rhs.addWidget(self.track_player_info_container)

        self.player_info_layout = QHBoxLayout()
        self.player_info_layout.addLayout(self.player_info_layout_lhs)
        self.player_info_layout.addLayout(self.player_info_layout_rhs)

        self.player_layout = QVBoxLayout()
        self.player_layout.addLayout(self.player_info_layout)
        self.player_layout.addLayout(self.playback_controls_layout)

        self.__show_placeholder()

    @Slot()
    def cleanup(self):
        """Cleanup method to be called before application exit"""
        if hasattr(self, "__poll_timer"):
            if self.__player_state_poll_timer is not None:
                self.__player_state_poll_timer.stop()
        if hasattr(self, "__thread_pool"):
            self.__thread_pool.kill()

    def __make_request(
        self, method: str, url: str
    ) -> tuple[bool, dict[str, Any] | bytes | None, urllib3.BaseHTTPResponse]:
        response = self.__http.request(method, url, timeout=2.0)
        if response.status != 200:
            return False, None, response

        if "application/json" in response.headers.get("Content-Type", ""):
            return True, json.loads(response.data.decode("utf-8")), response
        return True, response.data, response

    def __play_or_pause(self):
        try:
            if self.__state:
                ps = self.__state.player.playbackState
                if ps == "paused" or ps == "stopped":
                    self.__make_request("POST", f"{self.api_url}/player/play")
                elif ps == "playing":
                    self.__make_request("POST", f"{self.api_url}/player/stop")
        except Exception as e:
            print(f"Error in play command: {e}")

    def __spawn_play_or_pause(self):
        self.__thread_pool.spawn(self.__play_or_pause)

    def __stop(self):
        try:
            self.__make_request("POST", f"{self.api_url}/player/stop")
        except Exception as e:
            print(f"Error in stop command: {e}")

    def __spawn_stop(self):
        self.__thread_pool.spawn(self.__stop)

    def __update_state(self):
        success, data, _ = self.__make_request("GET", f"{self.api_url}/player")
        if not success or not data:
            return
        if isinstance(data, dict):
            st = BeefWeb.PlayerState.State(**data)
            if self.__state is not None:
                old_st = self.__state.model_copy()
                self.__state = st
                self.__playback_state_changed.emit(
                    st.player.playbackState != old_st.player.playbackState
                )

            else:
                self.__state = st
                self.__active_track_changed.emit(True)
                self.__playback_state_changed.emit(True)
        else:
            self.__state = None

    @Slot()
    def __spawn_update_state(self):
        self.__thread_pool.spawn(self.__update_state)

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

    @Slot()
    def __spawn_fetch_and_update_artwork(self):
        self.__thread_pool.spawn(self.__fetch_and_update_artwork)

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

    @Slot()
    def __spawn_get_active_track_info(self):
        self.__thread_pool.spawn(self.__get_active_track_info)

    @Slot()  # pyright: ignore[reportArgumentType]
    def __stream_active_track_info(self, changed: bool):
        if changed:
            sse_queue.put(self.__active_track)

    @Slot()  # pyright: ignore[reportArgumentType]
    def __handle_playback_state_changed(self, changed: bool):
        if self.__state is None:
            return
        if changed:
            ps = self.__state.player.playbackState
            sse_queue.put(ps)
            if ps == "stopped":
                self.stop_button.setDisabled(True)
                self.play_pause_button.setDisabled(False)
                self.play_pause_button.setIcon(self.play_icon)
            if ps == "playing":
                self.play_pause_button.setIcon(self.pause_icon)
                self.stop_button.setDisabled(False)
            if ps == "paused":
                self.play_pause_button.setIcon(self.play_icon)
                self.stop_button.setDisabled(False)

    @Slot()
    def __reschedule_greenlets(self):
        """
        Without this function, greenlets wont work. Here is why:

        In gevent, greenlets are cooperatively scheduled, meaning they voluntarily yield control to other greenlets.

        gevent.sleep(0) is special - it doesn't actually sleep, but it does two critical things:

        -   Forces the current greenlet to yield control back to the event loop
        -   Allows the event loop to process any pending greenlets in the queue

        Without this periodic sleep(0), greenlets are getting spawned but never getting a chance to run because:
        -   Qt's event loop runs on the main thread
        -   Greenlets need explicit scheduling points to switch between them
        -   The Qt timer is spawning greenlets, but nothing is telling gevent "now's a good time to run those greenlets"

        ```py
        # Without gevent.sleep(0):
        Timer tick -> Spawn greenlet -> Timer tick -> Spawn greenlet  # Greenlets never run!

        # With gevent.sleep(0):
        Timer tick -> Spawn greenlet -> sleep(0) -> Greenlet runs -> Timer tick -> ...
        ```
        So this function is essentially telling gevent "pause whatever you're doing and check if there are any other greenlets that need to run."

        Suggested way to use:
        ```py
        self.__poll_timer = QTimer(self)
        # update the ui etc.
        self.__poll_timer.timeout.connect(self.__spawn_update_state)
        self.__poll_timer.start(1000)

        self.__greenlet_timer = QTimer(self)
        self.__greenlet_timer.timeout.connect(self.__reschedule_greenlets)
        self.__greenlet_timer.start()
        ```
        Do not set the timer interval. From https://doc.qt.io/qt-6/qtimer.html#interval-prop:

        > The default value for interval is 0. A QTimer with a timeout interval of 0 will time out as soon as all the events in the window system's event queue have been processed.

        By force waking up the gevent for processing the greenlets in queue, we have instant response to the new work arriving, meaning there will be no artificial delays.

        There is pool size check before calling gevent.sleep to prevent unnecessary rescheduling, and, even then, calling gevent.sleep does not have any significant performance hits, at all,
        so there is no harm in running this function after event loop in main thread (QT) is done.
        """
        if len(self.__thread_pool.greenlets) > 0:
            gevent.sleep(0)
