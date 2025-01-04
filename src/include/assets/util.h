#include <qicon.h>
#include <qsize.h>
QIcon hex_to_icon(const unsigned char hex[], int size,
                  std::string format = "ICO", QSize svg_size = QSize(24,24));
QByteArray hex_to_byte(const unsigned char hex[], int size);
std::string hex_to_string(const unsigned char hex[]);