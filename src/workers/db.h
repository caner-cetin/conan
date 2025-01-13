#ifndef DB_H
#define DB_H
#include "sqlite3.h"
#include <nlohmann/json.hpp>
#include <qobject.h>
#include <qtmetamacros.h>

class DB : public QObject {
  Q_OBJECT
public:
  DB(QObject *parent = nullptr, const char *db_name = "Conan.db");
  sqlite3 *db;
  void create_all_tables_if_not_exists();

  const char *
  addTrack(const std::string &path, const std::string &artist,
           const std::string &title,
           const std::vector<std::pair<int, float>> &genrePredictions);

private:
  std::vector<uint8_t> compress_json(const nlohmann::json &features);
  nlohmann::json decompress_json(const std::vector<uint8_t> &compressed);
};
#endif