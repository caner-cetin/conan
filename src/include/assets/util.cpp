#include "util.h"
#include <qglobal.h>
#include <qicon.h>
#include <qimage.h>
#include <qsize.h>
QByteArray hex_to_byte(const unsigned char hex[], int size) {
  return QByteArray(reinterpret_cast<const char *>(hex), size);
}

QIcon hex_to_icon(const unsigned char hex[], int size, std::string format,
                  QSize svg_size) {

  QByteArray byte = hex_to_byte(hex, size);
  QImage image;
  if (!image.loadFromData(byte, format.c_str()))
    return QIcon();
  return QIcon(QPixmap::fromImage(image));
}
std::string hex_to_string(const unsigned char hex[]) {
  return std::string(reinterpret_cast<const char *>(hex));
}