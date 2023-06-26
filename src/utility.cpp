#include "utility.hpp"

#include <fstream>

bool file_exists(const std::string& filename) noexcept {
    return std::ifstream(filename).is_open();
}
