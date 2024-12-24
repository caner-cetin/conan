import json
import os
from typing import IO, Any, Iterable, final
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
    QFrame,
    QGroupBox,
    QHBoxLayout,
    QLabel,
    QLayout,
    QMainWindow,
    QPushButton,
    QScrollArea,
    QVBoxLayout,
    QWidget,
)
from urllib3.poolmanager import PoolManager

from backend import SSEMessage, sse_queue
from constants import ActiveTrack, AudioFeatures, BeefWeb


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
class SimilarTracksWidget(QGroupBox):
    def __init__(
        self,
        title: str = "Similar Tracks (select a track from the list above)",
        parent: QWidget | None = None,
    ) -> None:
        super().__init__(title, parent)
        self.track_widgets: list[SimilarTrackItemWidget] = []
        group_layout = QVBoxLayout(self)
        group_layout.setContentsMargins(5, 5, 5, 5)

        self.scroll_area = QScrollArea()
        self.scroll_area.setWidgetResizable(True)
        self.scroll_area.setHorizontalScrollBarPolicy(
            Qt.ScrollBarPolicy.ScrollBarAsNeeded
        )

        self.tracks_container = QWidget()

        self.tracks_layout = QVBoxLayout(self.tracks_container)
        self.tracks_layout.setSpacing(2)
        self.tracks_layout.setContentsMargins(0, 0, 0, 0)
        self.tracks_layout.addStretch()

        self.scroll_area.setWidget(self.tracks_container)

        group_layout.addWidget(self.scroll_area)

    def add_track(self, af: AudioFeatures):
        # Create track widget
        track_widget = SimilarTrackItemWidget(af)

        # Add separator if not the first item
        if len(self.track_widgets) > 0:
            separator = QFrame()
            separator.setFrameShape(QFrame.Shape.HLine)
            separator.setFrameShadow(QFrame.Shadow.Sunken)
            self.tracks_layout.insertWidget(self.tracks_layout.count() - 1, separator)

        self.tracks_layout.insertWidget(self.tracks_layout.count() - 1, track_widget)
        self.track_widgets.append(track_widget)

    def remove_track(self, index: int):
        """Remove a track at the specified index"""
        if 0 <= index < len(self.track_widgets):
            # Remove the track widget
            track_widget = self.track_widgets.pop(index)

            # Remove the separator if it exists (for all but the first item)
            if index > 0:
                # Get the separator (it's the widget before the track)
                separator = self.tracks_layout.itemAt(index * 2 - 1).widget()
                separator.deleteLater()
            elif len(self.track_widgets) > 0:
                # If removing first item and more items exist, remove the next separator
                separator = self.tracks_layout.itemAt(1).widget()
                separator.deleteLater()

            # Remove and delete the track widget
            track_widget.deleteLater()

    def clear_tracks(self, layout: QLayout | None = None):
        target = layout if layout else self.tracks_layout
        while target.count():
            item = target.takeAt(0)
            widget = item.widget()
            if widget is not None:
                widget.deleteLater()
            else:
                self.clear_tracks(item.layout())


@final
class SimilarTrackItemWidget(QWidget):
    play_track = Signal(str)  # file path
    queue_track = Signal(str)  # file path

    def __init__(
        self,
        af: AudioFeatures,
        parent: QWidget | None = None,
        f: Qt.WindowType = Qt.WindowType.Widget,
    ):
        super().__init__(parent, f)
        self.active_track = af

        layout = QHBoxLayout(self)
        layout.setContentsMargins(5, 2, 5, 2)

        btn_icon_size = QSize(12, 12)
        play_now_icon = QIcon("./assets/icons/play.svg")
        play_now_btn = QPushButton()
        play_now_btn.setIcon(play_now_icon)
        play_now_btn.setIconSize(btn_icon_size)

        queue_next_icon = QIcon("./assets/icons/queue-next.svg")
        queue_next_btn = QPushButton()
        queue_next_btn.setIcon(queue_next_icon)
        queue_next_btn.setIconSize(btn_icon_size)

        title_label = QLabel(f"{af.metadata['artist'][0]} - {af.metadata['title'][0]}")  # pyright: ignore [reportIndexIssue]
        title_label.setAlignment(
            Qt.AlignmentFlag.AlignRight | Qt.AlignmentFlag.AlignVCenter
        )
        layout.addWidget(title_label)
        layout.addStretch()
        layout.addWidget(play_now_btn)
        layout.addWidget(queue_next_btn)


@final
class Foobar2K(QObject):
    api_url: str | None
    __state: BeefWeb.PlayerState.State | None = None
    __active_track_changed = Signal(bool)
    __playback_state_changed = Signal(bool)
    __active_track: ActiveTrack | None = None
    __up_next: ActiveTrack | None = None
    __player_state_poll_timer: QTimer | None = None
    player_layout: QVBoxLayout
    similar_tracks: SimilarTracksWidget

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
        self.__playback_state_changed.connect(self.__handle_playback_state_changed)

        self.__thread_pool = Group()

    def __setup_ui(self):
        self.cover_art: bytes | None = None
        self.cover_art_scale = QSize(180, 180)

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
        self.skip_button = QPushButton()
        self.skip_button.clicked.connect(self.__spawn_skip)

        self.playback_control_icon_size = QSize(24, 24)
        self.pause_icon = QIcon("./assets/icons/pause.svg")
        self.play_icon = QIcon("./assets/icons/play.svg")
        self.stop_icon = QIcon("./assets/icons/stop.svg")
        self.skip_icon = QIcon("./assets/icons/skip.svg")
        self.play_pause_button.setIcon(self.pause_icon)
        self.play_pause_button.setIconSize(self.playback_control_icon_size)
        self.stop_button.setIcon(self.stop_icon)
        self.stop_button.setIconSize(self.playback_control_icon_size)
        self.skip_button.setIcon(self.skip_icon)
        self.skip_button.setIconSize(self.playback_control_icon_size)

        self.playback_controls.addButton(self.play_pause_button)
        self.playback_controls.addButton(self.stop_button)
        self.playback_controls.addButton(self.skip_button)
        self.playback_controls_layout.addWidget(self.play_pause_button)
        self.playback_controls_layout.addWidget(self.stop_button)
        self.playback_controls_layout.addWidget(self.skip_button)

        self.player_info_layout_lhs.addWidget(self.cover_art_container)
        self.player_info_layout_lhs.addLayout(self.playback_controls_layout)

        self.cover_art_placeholder = QMovie("./assets/no_cover_art.gif")
        self.cover_art_placeholder.setScaledSize(self.cover_art_scale)

        self.player_info_layout_rhs = QVBoxLayout()

        self.track_player_info_container = QWebEngineView()
        self.track_player_info_container.setMaximumHeight(200)
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

        self.similar_tracks = SimilarTracksWidget()

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
        self,
        method: str,
        url: str,
        body: bytes | IO[Any] | Iterable[bytes] | str | None = None,
    ) -> tuple[bool, dict[str, Any] | bytes | None, urllib3.BaseHTTPResponse]:
        response = self.__http.request(method, url, timeout=2.0, body=body)
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
                    self.__make_request("POST", f"{self.api_url}/player/pause")
        except Exception as e:
            print(f"Error in play command: {e}")

    def __spawn_play_or_pause(self):
        self.__thread_pool.spawn(self.__play_or_pause)

    def __skip(self):
        try:
            if self.__up_next:
                self.__make_request("POST", f"{self.api_url}/player/next")
        except Exception as e:
            print(f"Error in skip command: {e}")

    @Slot()
    def __spawn_skip(self):
        self.__thread_pool.spawn(self.__skip)

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
                self.__active_track_changed.emit(
                    (
                        st.player.activeItem.playlistId
                        != old_st.player.activeItem.playlistId
                    )
                    or (st.player.activeItem.index != old_st.player.activeItem.index)
                )
                self.__playback_state_changed.emit(
                    st.player.playbackState != old_st.player.playbackState
                )
                # todo: track queue change for up next

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
            columns_param = quote(json.dumps(BeefWeb.PlaylistItems.Item.query_columns))

            success, data, response = self.__make_request(
                "GET",
                (
                    f"{self.api_url}/playlists/"
                    f"{self.__state.player.activeItem.playlistId}/items/"
                    f"{self.__state.player.activeItem.index}:2?"
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
                for i, item in enumerate(playlist.playlistItems.items):
                    for j, _ in enumerate(item.columns):
                        playlist.playlistItems.items[i].columns[j] = (
                            playlist.playlistItems.items[i]
                            .columns[j]
                            .strip()
                            .replace('"', "", -1)
                        )
                queue = playlist.playlistItems.items
                self.__active_track = queue[0].to_active_track()
                sse_queue.put(SSEMessage(sent="ActiveTrack", data=self.__active_track))
                if len(queue) > 1:
                    self.__up_next = queue[1].to_active_track()
                    sse_queue.put(SSEMessage(sent="UpNext", data=self.__up_next))

    @Slot()
    def __spawn_get_active_track_info(self):
        self.__thread_pool.spawn(self.__get_active_track_info)

    @Slot()
    def __queue_track(self):
        """
        todo: implement dis 
        curl --header "Content-Type: application/json" \
        --request POST \
        --data '{"index":13, "async":true, "replace":false, "play":false, "items":["G:/Music/Conan/2012 - Monnos/04. Golden Axe.mp3"]}' \
        "http://uri/api/playlists/{playlist id}/items/add"
        with queue order coming from the similar tracks section inside the track analysis info
        how?
        i dont know good fucking luck
        """
        pass

    @Slot()  # pyright: ignore[reportArgumentType]
    def __handle_playback_state_changed(self, changed: bool):
        if self.__state is None:
            return
        if changed:
            ps = self.__state.player.playbackState
            sse_queue.put(SSEMessage(sent="PlaybackState", data=ps))
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
