#include "favicon.h"

#include <assets/favicon.h>

#include <QByteArray>
#include <QIcon>
#include <QImage>
QIcon loadFavIcon() {
  QByteArray byteArray(reinterpret_cast<const char*>(Resources::favicon),
                       Resources::favicon_size);
  QIcon icon;
  icon.addPixmap(QPixmap::fromImage(QImage::fromData(byteArray, "ICO")));
  return icon;
}