#include "db.h"
#include "conan_util.h"
#include <nlohmann/json.hpp>
#include <qapplication.h>
#include <qobject.h>
#include <qsettings.h>
#include <spdlog/spdlog.h>
#include <sqlite3.h>
#include <zlib.h>

using json = nlohmann::json;

DB::DB(QObject *parent, const char *db_name) : QObject(parent) {
  QSettings settings;
  sqlite3_open_v2(Settings::db_path(db_name).c_str(), &db,
                  SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
}

void DB::create_all_tables_if_not_exists() {
  const char *createTracks = R"(
            CREATE TABLE IF NOT EXISTS main.tracks (
                id BLOB PRIMARY KEY,
                path TEXT UNIQUE,
                artist TEXT,
                title TEXT,
                features BLOB,
                top_genres JSON,
                primary_genre_id INTEGER
            )
        )";
  conan_sqlite_exec_sql(db, createTracks);
  const char *createSimilarities = R"(
            CREATE TABLE IF NOT EXISTS main.track_similarities (
                track_a_id BLOB,
                track_b_id BLOB,
                similarity_score REAL,
                PRIMARY KEY(track_a_id, track_b_id),
                FOREIGN KEY(track_a_id) REFERENCES tracks(id),
                FOREIGN KEY(track_b_id) REFERENCES tracks(id)
            )
        )";
  conan_sqlite_exec_sql(db, createSimilarities);
  const char *createGenreIndex = R"(
            CREATE INDEX IF NOT EXISTS idx_primary_genre 
            ON tracks(primary_genre_id)
        )";
  conan_sqlite_exec_sql(db, createGenreIndex);
  const char *similarityScoreIndex = R"(
            CREATE INDEX IF NOT EXISTS idx_similarities_score 
            ON track_similarities(similarity_score DESC);
  )";
  conan_sqlite_exec_sql(db, similarityScoreIndex);
}

const char *
DB::addTrack(const std::string &path, const std::string &artist,
             const std::string &title,
             const std::vector<std::pair<int, float>> &genrePredictions) {
  json genresJson = json::array();
  for (size_t i = 0; i < std::min(size_t(10), genrePredictions.size()); i++) {
    genresJson.push_back({{"genre_id", genrePredictions[i].first},
                          {"probability", genrePredictions[i].second}});
  }

  const char *sql = R"(
            INSERT INTO tracks (path, artist, title, top_genres, primary_genre_id)
            VALUES (?, ?, ?, ?, ?)
        )";
}

std::vector<uint8_t> DB::compress_json(const nlohmann::json &features) {
  std::string jsonStr = features.dump();

  uLong compressedSize = compressBound(jsonStr.size());
  std::vector<uint8_t> compressed(compressedSize);

  compress2(compressed.data(), &compressedSize, (const Bytef *)jsonStr.data(),
            jsonStr.size(), Z_BEST_COMPRESSION);

  compressed.resize(compressedSize);
  return compressed;
}

nlohmann::json DB::decompress_json(const std::vector<uint8_t> &compressed) {
  // 4 should be reasonable to start
  std::vector<uint8_t> decompressed(compressed.size() * 4);
  uLong decompressedSize = decompressed.size();

  uncompress(decompressed.data(), &decompressedSize, compressed.data(),
             compressed.size());

  decompressed.resize(decompressedSize);
  return nlohmann::json::parse(std::string(
      reinterpret_cast<char *>(decompressed.data()), decompressedSize));
}