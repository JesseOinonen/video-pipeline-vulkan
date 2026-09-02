#include "hex_io.hpp"

#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <string>

std::vector<uint16_t> readHex(const char* path) {
    // ate: start at the end so tellg() gives the size without a second syscall.
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error(std::string("Failed to open hex file: ") + path);
    }

    const std::streamsize size = file.tellg();
    file.seekg(0);

    std::string text(static_cast<size_t>(size), '\0');
    if (!file.read(text.data(), size)) {
        throw std::runtime_error(std::string("Failed to read hex file: ") + path);
    }

    std::vector<uint16_t> values;
    values.reserve(static_cast<size_t>(size) / 5 + 1);   // "xxxx\n" per entry

    // strtoul skips leading whitespace, newlines included, so one pass over the
    // whole buffer handles the line-per-value layout without splitting lines.
    const char* p = text.c_str();
    while (*p != '\0') {
        char* end = nullptr;
        const unsigned long v = std::strtoul(p, &end, 16);
        if (end == p) {
            break;              // nothing left but trailing whitespace
        }
        if (v > 0xFFFFul) {
            throw std::runtime_error(std::string("Value does not fit in 16 bits in ") + path);
        }
        values.push_back(static_cast<uint16_t>(v));
        p = end;
    }

    return values;
}
