#include <memory>
#include <qicon.h>
std::unique_ptr<QIcon> hex_to_icon(const unsigned char hex[], int size,
                                   std::string format = "ICO");
QByteArray hex_to_byte(const unsigned char hex[], int size);