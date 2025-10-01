#include "utils.hpp"
#include <fstream>

std::string ReadContentFromFile(std::filesystem::path const& path) {
    std::ifstream file{path, std::ios::in | std::ios::binary};
    if(!file) {
        throw std::runtime_error("Failed to open file: " + path.string());
    }

    std::string content{(std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>()};
    return content;
}