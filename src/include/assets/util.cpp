#include "util.h"
#include <memory>
#include <qglobal.h>
#include <qicon.h>
QByteArray hex_to_byte(const unsigned char hex[], int size) {
  return QByteArray(reinterpret_cast<const char *>(hex), size);
}

std::unique_ptr<QIcon> hex_to_icon(const unsigned char hex[], int size,
                                   std::string format) {
  auto byte = hex_to_byte(hex, size);
  auto image = std::make_unique<QImage>();
  image->loadFromData(byte, format.c_str());
  return std::make_unique<QIcon>(QPixmap::fromImage(*image));
};