from flask.templating import render_template_string
from gevent import monkey
from gevent.queue import Queue

from templates import TrackPlayerInfoTemplate

monkey.patch_all()

from flask import Flask, Response, request
from gevent.pywsgi import WSGIServer

from constants import ActiveTrack, PlaybackState

app = Flask(__name__)

sse_queue = Queue()


@app.route("/")
def index():
    return render_template_string(TrackPlayerInfoTemplate().template)


@app.route("/sse")
def events():
    def generate():
        while True:
            data: PlaybackState | ActiveTrack = sse_queue.get(block=True, timeout=None)
            playback_status_html: str = ""
            if isinstance(data, str):  # playback state
                match data:
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
            if isinstance(data, ActiveTrack):
                yield f"event: Title\ndata: <div class='track-title'>{data.artist} - {data.title}</div>\n\n"
                yield f"event: Album\ndata: <div class='track-details'>{data.album} ({data.track_number} / {data.total_tracks})</div>\n\n"

    response = Response(generate(), mimetype="text/event-stream")
    response.headers.add("Access-Control-Allow-Origin", "http://127.0.0.1:31311")
    return response


def run_server():
    http_server = WSGIServer(("127.0.0.1", 31311), app)
    http_server.serve_forever()
