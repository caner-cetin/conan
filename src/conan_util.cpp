#include <QThreadPool>
#include <algorithm>
#include <conan_util.h>
#include <cstring>
#include <curl/curl.h>
#include <locale>
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

CurlRAII::CurlRAII() : curl(curl_easy_init()) {
  if (!curl) {
    // todo: handle more gracefully
    throw std::runtime_error("CURL init failed");
  }
  // #ifndef DEBUG
  //   curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
  // #endif
}
CurlRAII::~CurlRAII() {
  if (curl) {
    curl_easy_cleanup(curl);
  }
}

void CurlRAII::set_empty_post_request() {
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
