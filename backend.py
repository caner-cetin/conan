from dataclasses import dataclass
from typing import Literal

from flask.templating import render_template_string
from gevent import monkey
from gevent.queue import Queue

from templates import TrackPlayerInfoTemplate

monkey.patch_all()

from flask import Flask, Response
from gevent.pywsgi import WSGIServer

from constants import ActiveTrack, PlaybackState

app = Flask(__name__)

sse_queue = Queue()


@dataclass
class SSEMessage:
    sent: Literal["ActiveTrack", "PlaybackState", "UpNext"]
    data: PlaybackState | ActiveTrack


@app.route("/")
def index():
    return render_template_string(TrackPlayerInfoTemplate().template)


@app.route("/sse")
def events():
    def generate():
        while True:
            message: SSEMessage = sse_queue.get(block=True, timeout=None)
            match message.sent:
                case "ActiveTrack" | "UpNext":
                    if isinstance(message.data, ActiveTrack):
                        active = message.sent == "ActiveTrack"
                        yield f"event: {'Title' if active else 'UpNextTitle'}\ndata: <div class={'track-title' if active else ''}>{message.data.artist} - {message.data.title}</div>\n\n"
                        yield f"event: {'Album' if active else 'UpNextAlbum'}\ndata: <div class='track-details'>{message.data.album} ({message.data.track_number} / {message.data.total_tracks if message.data.total_tracks != 0 else '?'})</div>\n\n"
                case "PlaybackState":
                    playback_status_html = ""
                    match message.data:
                        case "stopped":
                            playback_status_html = (
                                """<span class="playback-stopped">Stopped</span>"""
                            )
                        case "paused":
                            playback_status_html = (
                                """<span class='playback-paused'>Paused</span>"""
                            )
                        case "playing":
                            playback_status_html = (
                                """<span class="playback-playing">Playing</span>"""
                            )
                        case _:  # noqa: E701
                            pass
                    yield f"event: PlaybackState\ndata: {playback_status_html}\n\n"
                case _:
                    pass

    response = Response(generate(), mimetype="text/event-stream")
    response.headers.add("Access-Control-Allow-Origin", "http://127.0.0.1:31311")
    return response


def run_server():
    http_server = WSGIServer(("127.0.0.1", 31311), app)
    http_server.serve_forever()
