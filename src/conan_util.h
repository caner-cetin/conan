#ifndef CONAN_UTIL_H
#define CONAN_UTIL_H
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
#endif