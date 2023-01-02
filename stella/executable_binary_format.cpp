#pragma once

// utilities for reading executable binary files

#include <fstream>
#include <vector>

// reads the file and returns the contents
std::vector<char> readBinaryFile(std::string const& path) {
    std::vector<char> res;

    std::ifstream file(path.c_str(), ios::in|ios::binary);

    if (file.is_open()) {
        file.seekg(0, ios::end);
        size_t size = file.tellg();
        file.seekg(0, ios::beg);

        res.resize(size);
        file.read(res.data(), size);
    }

    return res;
}