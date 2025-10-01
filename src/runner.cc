#include "runner.hpp"
#include <cstdio>
#include <vector>

void RunProgram(std::string const& program) {
    std::vector<uint8_t> memory(30 * 1024, 0); // 30KB memory
    size_t pointer = 0;
    size_t program_counter = 0;

    while(program_counter < program.size()) {
        switch(program[program_counter]) {
            case '>':
                ++pointer;
                break;
            case '<':
                --pointer;
                break;
            case '+':
                ++memory[pointer];
                break;
            case '-':
                --memory[pointer];
                break;
            case '.':
                std::putchar(memory[pointer]);
                break;
            case ',':
                memory[pointer] = std::getchar();
                break;
            case '[':
                {
                    if(memory[pointer] == 0) {
                        int32_t depth = 1;

                        while(depth > 0) {
                            ++program_counter;
                            if(program[program_counter] == '[') {
                                ++depth;
                            } else if(program[program_counter] == ']') {
                                --depth;
                            }
                        }
                        --program_counter; // Adjust for the upcoming increment
                    }
                }
                break;
            case ']':
                {
                    if(memory[pointer] != 0) {
                        int32_t depth = 1;

                        while(depth > 0) {
                            --program_counter;
                            if(program[program_counter] == '[') {
                                --depth;
                            } else if(program[program_counter] == ']') {
                                ++depth;
                            }
                        }
                        // No need to adjust program_counter here
                    }
                }
                break;
            default:
                break; // Ignore other characters
        }

        ++program_counter;
    }
}