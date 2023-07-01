#ifndef UTILITY_HPP
#define UTILITY_HPP

#include <string>

bool file_exists(const std::string& filename) noexcept;

std::string read_file(const std::string& filepath);

std::string read_neighbor_file(const std::string& basepath, const std::string& filename);

#endif
