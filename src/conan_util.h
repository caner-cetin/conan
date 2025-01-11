#ifndef CONAN_UTIL_H
#define CONAN_UTIL_H
#include <QThreadPool>
#include <curl/curl.h>
#include <functional>
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

class CurlRAII {
public:
  CurlRAII();
  ~CurlRAII();
  // Allow using this object as if it were a CURL*
  operator CURL *() { return curl; }
  // prevent copying
  CurlRAII(const CurlRAII &) = delete;
  CurlRAII &operator=(const CurlRAII &) = delete;

  void set_empty_post_request();

private:
  CURL *curl;
};

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
#endif