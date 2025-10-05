#include "builder.hpp"
#include "ir.hpp"
#include <stdexcept>

IRProgram BuildIR(std::string const& source) {
    IRProgram program{};
    std::vector<int32_t> loop_stack{};

    for(char const c : source ){
        switch(c) {
            case '>':
                program.emplace_back(MovePtr{1});
                break;
            case '<':
                program.emplace_back(MovePtr{-1});
                break;
            case '+':
                program.emplace_back(ChangeValue{1});
                break;
            case '-':
                program.emplace_back(ChangeValue{-1});
                break;
            case '.':
                program.emplace_back(OutputCharacter{});
                break;
            case ',':
                program.emplace_back(InputCharacter{});
                break;
            case '[':
                loop_stack.push_back(static_cast<int32_t>(program.size()));
                program.emplace_back(LoopStart{0}); // Placeholder, will set jump_offset later
                break;
            case ']':
                if(loop_stack.empty()) {
                    throw std::runtime_error("Unmatched ']' in source code");
                }
                else {
                    int32_t last_loop = loop_stack.back();
                    loop_stack.pop_back();

                    int32_t current_pos = static_cast<int32_t>(program.size());

                    program[last_loop] = LoopStart{current_pos - last_loop};
                    program.emplace_back(LoopEnd{last_loop - current_pos});
                }
        }
    }

    return program;
}