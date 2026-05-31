#include "runner.hpp"
#include <cstdio>
#include <vector>
#include <stdexcept>
#include <format>
#include <algorithm>

namespace {
    // Helper to get cell value based on cell size
    template<typename T>
    void increment_cell(T& cell, bool wrap) {
        if constexpr (std::is_same_v<T, uint8_t>) {
            if (wrap) {
                ++cell;
            } else {
                cell = static_cast<uint8_t>(std::min(255u, static_cast<unsigned>(cell) + 1));
            }
        } else if constexpr (std::is_same_v<T, uint16_t>) {
            if (wrap) {
                ++cell;
            } else {
                cell = static_cast<uint16_t>(std::min(65535u, static_cast<unsigned>(cell) + 1));
            }
        } else if constexpr (std::is_same_v<T, uint32_t>) {
            if (wrap) {
                ++cell;
            } else {
                cell = std::min(UINT32_MAX, cell + 1);
            }
        }
    }

    template<typename T>
    void decrement_cell(T& cell, bool wrap) {
        if constexpr (std::is_same_v<T, uint8_t>) {
            if (wrap) {
                --cell;
            } else {
                cell = 0;
            }
        } else if constexpr (std::is_same_v<T, uint16_t>) {
            if (wrap) {
                --cell;
            } else {
                cell = 0;
            }
        } else if constexpr (std::is_same_v<T, uint32_t>) {
            if (wrap) {
                --cell;
            } else {
                cell = 0;
            }
        }
    }

    // Run program with specific cell type
    template<typename CellType>
    void run_program_typed(std::string const& program, RunConfig const& config) {
        std::vector<CellType> memory(config.tape_size / sizeof(CellType), 0);
        size_t pointer = 0;
        size_t program_counter = 0;
        const size_t max_index = memory.size() - 1;

        while (program_counter < program.size()) {
            char c = program[program_counter];

            switch (c) {
                case '>':
                    if (config.bounds_check && pointer >= max_index) {
                        throw std::runtime_error(std::format(
                            "Runtime error: Pointer out of bounds at position {} (tried to move beyond cell {})",
                            program_counter, pointer
                        ));
                    }
                    ++pointer;
                    break;
                case '<':
                    if (config.bounds_check && pointer == 0) {
                        throw std::runtime_error(std::format(
                            "Runtime error: Pointer out of bounds at position {} (tried to move before cell 0)",
                            program_counter
                        ));
                    }
                    --pointer;
                    break;
                case '+':
                    increment_cell(memory[pointer], config.overflow_wrap);
                    break;
                case '-':
                    decrement_cell(memory[pointer], config.overflow_wrap);
                    break;
                case '.':
                    std::putchar(static_cast<unsigned char>(memory[pointer] & 0xFF));
                    break;
                case ',':
                    memory[pointer] = static_cast<CellType>(std::getchar());
                    break;
                case '[':
                    if (memory[pointer] == 0) {
                        int32_t depth = 1;
                        while (depth > 0 && program_counter + 1 < program.size()) {
                            ++program_counter;
                            if (program[program_counter] == '[') {
                                ++depth;
                            } else if (program[program_counter] == ']') {
                                --depth;
                            }
                        }
                    }
                    break;
                case ']':
                    if (memory[pointer] != 0) {
                        int32_t depth = 1;
                        while (depth > 0 && program_counter > 0) {
                            --program_counter;
                            if (program[program_counter] == '[') {
                                --depth;
                            } else if (program[program_counter] == ']') {
                                ++depth;
                            }
                        }
                    }
                    break;
                default:
                    break; // Ignore other characters
            }

            ++program_counter;
        }
    }
}

void RunProgram(std::string const& program, RunConfig const& config) {
    switch (config.cell_size) {
        case 8:
            run_program_typed<uint8_t>(program, config);
            break;
        case 16:
            run_program_typed<uint16_t>(program, config);
            break;
        case 32:
            run_program_typed<uint32_t>(program, config);
            break;
        default:
            throw std::runtime_error(std::format("Invalid cell size: {}. Supported sizes: 8, 16, 32", config.cell_size));
    }
}
