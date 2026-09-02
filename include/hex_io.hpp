#pragma once

#include <cstdint>
#include <vector>

// Read a file of 16-bit hex words, one per line, into a vector.
//
// This is the format the RTL project's testbench uses for both its input image
// and its golden outputs: four hex digits per line, no header, raster order.
// Whitespace and blank lines are skipped.
//
// Throws if the file cannot be opened or contains a value that does not fit in
// 16 bits.
std::vector<uint16_t> readHex(const char* path);
