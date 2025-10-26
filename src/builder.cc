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

IRProgram OptimizeIR(IRProgram const& program) {
    if(program.size() < 2) {
        return program;
    }

    IRProgram optimized{};
    std::vector<int32_t> loop_stack{};  // Track LoopStarts positions for fixing offsets
    
    for(size_t i = 0; i < program.size(); ++i) {
        IRInstruction current = program[i];
        
        if(std::holds_alternative<MovePtr>(current)) {
            int32_t total_offset = std::get<MovePtr>(current).offset();
            
            while(i + 1 < program.size() && std::holds_alternative<MovePtr>(program[i + 1])) {
                ++i;
                total_offset += std::get<MovePtr>(program[i]).offset();
            }
            
            if(total_offset != 0) {
                optimized.emplace_back(MovePtr{total_offset});
            }
        }
        else if(std::holds_alternative<ChangeValue>(current)) {
            int32_t total_delta = std::get<ChangeValue>(current).delta();
            
            while(i + 1 < program.size() && std::holds_alternative<ChangeValue>(program[i + 1])) {
                ++i;
                total_delta += std::get<ChangeValue>(program[i]).delta();
            }
            
            if(total_delta != 0) {
                optimized.emplace_back(ChangeValue{total_delta});
            }
        }
        else if(std::holds_alternative<LoopStart>(current)) {
            loop_stack.push_back(static_cast<int32_t>(optimized.size()));
            optimized.emplace_back(LoopStart{0}); // Placeholder
        }
        else if(std::holds_alternative<LoopEnd>(current)) {
            if(!loop_stack.empty()) {
                int32_t loop_start_pos = loop_stack.back();
                loop_stack.pop_back();
                
                int32_t current_pos = static_cast<int32_t>(optimized.size());
                
                optimized[loop_start_pos] = LoopStart{current_pos - loop_start_pos};
                
                optimized.emplace_back(LoopEnd{loop_start_pos - current_pos});
            }
        }
        // Copy other instructions as-is
        else {
            optimized.emplace_back(current);
        }
    }
    
    return optimized;
}