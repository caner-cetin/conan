import os
from typing import final
from PySide6.QtGui import QColor
from PySide6.QtWebEngineCore import QWebEngineSettings
from PySide6.QtWebEngineWidgets import QWebEngineView
from PySide6.QtCore import QObject, Slot
from PySide6.QtWidgets import QMainWindow
import requests


@final
class TrackInfo(QWebEngineView):
  def __init__(self):
    super().__init__();
    self.track_info_page = self.page()
    self.track_info_page.setBackgroundColor(QColor(0,0,0,0))
    self.track_info_settings = self.track_info_page.settings()
    self.track_info_settings.setAttribute(QWebEngineSettings.WebAttribute.JavascriptEnabled, True)
    self.track_info_settings.setAttribute(QWebEngineSettings.WebAttribute.LocalContentCanAccessRemoteUrls, True)
    

@final
class Foobar2K(QObject):
  api_url: str | None
  def __init__(self, parent: QMainWindow) -> None:
    self.api_url = os.getenv("API_URL")
    if self.api_url is None:
      raise Exception("Api url for BeefWeb  is not set in .env")
    super().__init__(parent=parent)
   
  @Slot()
  def play(self):
    try:
      response = requests.post(f"{self.api_url}/player/play")
      response.raise_for_status()
    except requests.RequestException as e:
      print(str(e))