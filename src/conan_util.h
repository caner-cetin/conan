#ifndef CONAN_UTIL_H
#define CONAN_UTIL_H
#include <QThreadPool>
#include <curl/curl.h>
#include <filesystem>
#include <functional>
#include <qicon.h>
#include <qsettings.h>
#include <sqlite3.h>
#include <string>
#include <vector>
// https://stackoverflow.com/a/24315631/22757599
std::string &ReplaceAll(std::string &str, const std::string &from,
                        const std::string &to);
std::vector<std::string> split(std::string &s, const std::string &delimiter);
// https://stackoverflow.com/a/29892589/22757599
std::string &ltrim(std::string &str);
std::string &rtrim(std::string &str);
std::string &trim(std::string &str);

class conan_curl {
public:
  conan_curl();
  ~conan_curl();
  /**
   * @brief allows using this object as CURL*
   *
   * @return CURL *
   */
  operator CURL *() { return curl; }
  /**
   * @warning Constructing a new conan_curl from existing object is disallowed
   */
  conan_curl(const conan_curl &) = delete;
  /**
   * @warning Copying conan_sqlite_stmt object is disallowed
   */
  conan_curl &operator=(const conan_curl &) = delete;

  void set_empty_post_request();

private:
  CURL *curl;
};

class conan_sqlite_stmt {
public:
  /**
   * @brief Construct a new conan sqlite stmt object
   *
   * @param db
   * @param sql utf-8 encoded sql statement
   */
  conan_sqlite_stmt(sqlite3 *db, const char *sql);
  ~conan_sqlite_stmt();
  /**
   * @brief allows using this object as sqlite3_stmt*
   *
   * @return sqlite3_stmt *
   */
  operator sqlite3_stmt *() { return stmt; }
  /**
   * @warning Constructing a new conan_sqlite_stmt from existing object is
   * disallowed
   */
  conan_sqlite_stmt(const conan_sqlite_stmt &) = delete;
  /**
   * @warning Copying conan_sqlite_stmt object is disallowed
   */
  conan_sqlite_stmt &operator=(const conan_sqlite_stmt &) = delete;

private:
  sqlite3_stmt *stmt;
};
/**
 * @brief prepares and executes the sql statement
 *
 * @throws runtime_error if execution is not successful or sqlite3_step() has
 * not finished executing, runtime error will be thrown with the extended error
 * code and error message
 * @param db
 * @param sql
 */
void conan_sqlite_exec_sql(sqlite3 *db, const char *sql);

size_t CurlWrite_CallbackFunc_StdString(void *contents, size_t size,
                                        size_t nmemb, std::string *s);
size_t CurlWrite_CallbackFunc_StdBytes(void *contents, size_t size,
                                       size_t nmemb, void *userp);
/**
 * @brief invoke a function within global thread pool of Qt
 *
 * @tparam Func
 * @param func a function that takes nothing and returns nothing
 */
template <typename Func> void run_function_non_blocking(Func func) {
  std::function<void()> func_wrapper = func;
  QThreadPool::globalInstance()->start([func_wrapper]() { func_wrapper(); });
}

/**
 * @brief returns full path of audio files
 *
 * extensions: .mp3, .wav, flac, .ogg, .m4a
 *
 * modified from https://stackoverflow.com/a/59467741/22757599
 *
 * @param directory starting path
 */
std::vector<std::string> find_audio_files(std::filesystem::path directory);

inline QSettings settings;

class Settings {
public:
  static inline const char *DB_PATH_SETTING_KEY = "db_path";
  static std::string db_path(const char *db_name = "Conan.db");
  static void update_settings();
  static void change_settings(QSettings);
};

std::vector<unsigned char>
decode_compressed_base64(const char *input, size_t length,
                         const unsigned int compressed_size);
std::function<std::vector<unsigned char>()>
decompress_base64_encoded_zlib_compressed_data(
    const char compressed_data[], const unsigned int base64_size,
    const unsigned int compressed_size, const unsigned int original_size);

QByteArray hex_to_byte(const unsigned char hex[], int size);
QIcon hex_to_icon(const unsigned char hex[], int size, std::string format);
std::string hex_to_string(const unsigned char hex[]);
#endif