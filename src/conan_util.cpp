#include <QThreadPool>
#include <algorithm>
#include <conan_util.h>
#include <cstring>
#include <curl/curl.h>
#include <fmt/core.h>
#include <locale>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>
#include <spdlog/spdlog.h>
#include <sqlite3.h>
#include <stdexcept>
#include <zlib.h>
std::string &ReplaceAll(std::string &str, const std::string &from,
                        const std::string &to) {
  size_t start_pos = 0;
  while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
    str.replace(start_pos, from.length(), to);
    start_pos += to.length();
  }
  return str;
}

std::vector<std::string> split(std::string &s, const std::string &delimiter) {
  std::vector<std::string> tokens;
  size_t pos = 0;
  std::string token;
  while ((pos = s.find(delimiter)) != std::string::npos) {
    token = s.substr(0, pos);
    tokens.push_back(token);
    s.erase(0, pos + delimiter.length());
  }
  tokens.push_back(s);

  return tokens;
}

std::string &ltrim(std::string &str) {
  auto it2 = std::find_if(str.begin(), str.end(), [](char ch) {
    return !std::isspace<char>(ch, std::locale::classic());
  });
  str.erase(str.begin(), it2);
  return str;
}

std::string &rtrim(std::string &str) {
  auto it1 = std::find_if(str.rbegin(), str.rend(), [](char ch) {
    return !std::isspace<char>(ch, std::locale::classic());
  });
  str.erase(it1.base(), str.end());
  return str;
}

std::string &trim(std::string &str) { return ltrim(rtrim(str)); }

// https://stackoverflow.com/a/36401787/22757599
size_t CurlWrite_CallbackFunc_StdString(void *contents, size_t size,
                                        size_t nmemb, std::string *s) {
  size_t newLength = size * nmemb;
  try {
    s->append((char *)contents, newLength);
  } catch (std::bad_alloc &e) {
    // handle memory problem
    return 0;
  }
  return newLength;
}

// write data must be the type of std::vector<unsigned char>
size_t CurlWrite_CallbackFunc_StdBytes(void *contents, size_t size,
                                       size_t nmemb, void *userp) {
  size_t realsize = size * nmemb;
  std::vector<unsigned char> *mem = (std::vector<unsigned char> *)userp;
  size_t current_size = mem->size();
  mem->resize(current_size + realsize);
  std::memcpy(mem->data() + current_size, contents, realsize);

  return realsize;
}

conan_curl::conan_curl() : curl(curl_easy_init()) {
  if (!curl) {
    // todo: handle more gracefully
    throw std::runtime_error("CURL init failed");
  }
}
conan_curl::~conan_curl() {
  if (curl) {
    curl_easy_cleanup(curl);
  }
}

void conan_curl::set_empty_post_request() {
  curl_easy_setopt(curl, CURLOPT_POST, 1);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, 0);
  // some servers does not handle the case of content length being zero on post
  // requests gracefully and then curl expects for a Expect: 100 Continue
  // response so we just say "dont expect anything curl, we are empty handed and
  // that girl wont comeback to you"
  struct curl_slist *headers = nullptr;
  headers = curl_slist_append(headers, "Expect:");
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
}

std::vector<std::string> find_audio_files(std::filesystem::path directory) {
  const std::vector<std::string> extensions{".mp3", ".wav", ".flac", ".ogg",
                                            ".m4a"};

  std::vector<std::string> files;
  std::vector<std::string> audio_files;

  // Get all path names as string
  std::transform(std::filesystem::directory_iterator(directory), {},
                 std::back_inserter(files),
                 [](const std::filesystem::directory_entry &de) {
                   return de.path().string();
                 });

  // Output all files with the specified extensions
  std::copy_if(files.begin(), files.end(), std::back_inserter(audio_files),
               [&extensions](const std::string &p) {
                 // Check if the file has one of the specified extensions
                 return std::any_of(extensions.begin(), extensions.end(),
                                    [&p](const std::string &ext) {
                                      return p.size() >= ext.size() &&
                                             p.compare(p.size() - ext.size(),
                                                       ext.size(), ext) == 0;
                                    });
               });
  return audio_files;
}

std::string Settings::db_path(const char *db_name) {
  const auto default_path =
      (std::filesystem::current_path() / db_name).string();
  QString path = settings.value(DB_PATH_SETTING_KEY).toString();
  return path.isEmpty() ? default_path : path.toStdString().c_str();
}

conan_sqlite_stmt::conan_sqlite_stmt(sqlite3 *db, const char *sql) {
  sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, nullptr);
}

// finalize will also free the stmt
conan_sqlite_stmt::~conan_sqlite_stmt() { sqlite3_finalize(stmt); }

void conan_sqlite_exec_sql(sqlite3 *db, const char *sql) {
  conan_sqlite_stmt stmt(db, sql);
  int res = sqlite3_step(stmt);
  bool is_ok = res == SQLITE_OK;
  bool finished_executing = res == SQLITE_DONE;
  if (!is_ok && !finished_executing) {
    throw std::runtime_error(
        fmt::format("error executing sql, code: {}, error: {}",
                    sqlite3_extended_errcode(db), sqlite3_errmsg(db)));
  }
}

std::vector<unsigned char>
decode_compressed_base64(const char *input, size_t length,
                         const unsigned int compressed_size) {
  {
    BIO *b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);

    BIO *bmem = BIO_new_mem_buf(input, length);
    BIO *bio = BIO_push(b64, bmem);

    std::vector<unsigned char> result(compressed_size);
    int decoded_size = BIO_read(bio, result.data(), length);

    BIO_free_all(bio);

    if (decoded_size < 0) {
      {
        throw std::runtime_error("Base64 decode failed");
      }
    }

    if (decoded_size != compressed_size) {
      {
        throw std::runtime_error("Decoded size mismatch: got " +
                                 std::to_string(decoded_size) + " expected " +
                                 std::to_string(compressed_size));
      }
    }

    return result;
  }
}

std::function<std::vector<unsigned char>()>
decompress_base64_encoded_zlib_compressed_data(
    const char compressed_data[], const unsigned int base64_size,
    const unsigned int compressed_size, const unsigned int original_size) {
  return [compressed_data, base64_size, compressed_size, original_size]() {
    // First decode base64
    auto decoded =
        decode_compressed_base64(compressed_data, base64_size, compressed_size);

    // Initialize decompression stream
    z_stream strm;
    std::memset(&strm, 0, sizeof(strm));

    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;

    strm.avail_in = decoded.size();
    strm.next_in = decoded.data();

    int ret = inflateInit2(&strm, 15);
    if (ret != Z_OK) {
      throw std::runtime_error("Failed to initialize zlib");
    }

    // Create output buffer
    std::vector<unsigned char> decompressed_data(original_size);
    strm.avail_out = original_size;
    strm.next_out = decompressed_data.data();

    // Decompress
    ret = inflate(&strm, Z_FINISH);
    inflateEnd(&strm);

    if (ret != Z_STREAM_END) {
      throw std::runtime_error(
          "Decompression failed: " + std::to_string(ret) +
          " (decoded size: " + std::to_string(decoded.size()) +
          ", avail_in: " + std::to_string(strm.avail_in) +
          ", avail_out: " + std::to_string(strm.avail_out) + ")");
    }

    if (strm.total_out != original_size) {
      throw std::runtime_error("Size mismatch after decompression");
    }

    return decompressed_data;
  };
}

QByteArray hex_to_byte(const unsigned char hex[], int size) {
  return QByteArray(reinterpret_cast<const char *>(hex), size);
}

QIcon hex_to_icon(const unsigned char hex[], int size, std::string format) {
  QByteArray byte = hex_to_byte(hex, size);
  QPixmap pixmap;
  {
    QImage image;
    if (!image.loadFromData(byte, format.c_str())) {
      return QIcon();
    }
    pixmap = QPixmap::fromImage(image);
  }
  return QIcon(pixmap);
}
std::string hex_to_string(const unsigned char hex[]) {
  return std::string(reinterpret_cast<const char *>(hex));
}