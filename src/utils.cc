#include "utils.hpp"
#include <fstream>
#include <iostream>

std::string ReadContentFromFile(std::filesystem::path const& path) {
    std::ifstream file{path, std::ios::in};
    if(!file) {
        throw std::runtime_error("Failed to open file: " + path.string());
    }

    std::string content{(std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>()};
    return content;
}

std::string ReadContentFromStdin() {
    std::string content{(std::istreambuf_iterator<char>(std::cin)), std::istreambuf_iterator<char>()};
    return content;
}