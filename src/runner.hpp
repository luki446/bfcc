#pragma once

#include <string>
#include <cstdint>

struct RunConfig {
    size_t tape_size = 30 * 1024;       // Default 30KB
    size_t cell_size = 8;                // Default 8-bit cells
    bool bounds_check = false;           // Enable bounds checking
    bool overflow_wrap = true;           // Cell values wrap on overflow (false = saturate)
};

void RunProgram(std::string const& program, RunConfig const& config = {});
