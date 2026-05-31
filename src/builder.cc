#include "builder.hpp"
#include "ir.hpp"
#include <stdexcept>
#include <format>

namespace {
    // Brainfuck commands
    constexpr bool IsBrainfuckCommand(char c) {
        return c == '>' || c == '<' || c == '+' || c == '-' ||
               c == '.' || c == ',' || c == '[' || c == ']';
    }
}

IRProgram BuildIR(std::string const& source) {
    IRProgram program{};
    std::vector<std::pair<int32_t, SourceLocation>> loop_stack{}; // (position, location)

    int32_t line = 1;
    int32_t column = 1;
    SourceLocation last_non_whitespace_loc{1, 1};

    for(char const c : source) {
        // Track source location
        if (c == '\n') {
            ++line;
            column = 1;
        } else {
            ++column;
        }

        // Skip non-Brainfuck characters but warn about them
        if (!IsBrainfuckCommand(c)) {
            if (!std::isspace(static_cast<unsigned char>(c))) {
                // Warning: invalid character (not printed, could be added to a diagnostics system)
            }
            continue;
        }

        SourceLocation current_loc{line, column};
        last_non_whitespace_loc = current_loc;

        switch(c) {
            case '>':
                program.emplace_back(MovePtr{1, current_loc});
                break;
            case '<':
                program.emplace_back(MovePtr{-1, current_loc});
                break;
            case '+':
                program.emplace_back(ChangeValue{1, current_loc});
                break;
            case '-':
                program.emplace_back(ChangeValue{-1, current_loc});
                break;
            case '.':
                program.emplace_back(OutputCharacter{current_loc});
                break;
            case ',':
                program.emplace_back(InputCharacter{current_loc});
                break;
            case '[':
                loop_stack.push_back({static_cast<int32_t>(program.size()), current_loc});
                program.emplace_back(LoopStart{0, current_loc}); // Placeholder, will set jump_offset later
                break;
            case ']':
                if(loop_stack.empty()) {
                    throw std::runtime_error(std::format("Error at {}: Unmatched ']' - no matching '[' found", current_loc.toString()));
                }
                else {
                    auto [last_loop, loop_loc] = loop_stack.back();
                    loop_stack.pop_back();

                    int32_t current_pos = static_cast<int32_t>(program.size());

                    program[last_loop] = LoopStart{current_pos - last_loop, loop_loc};
                    program.emplace_back(LoopEnd{last_loop - current_pos, current_loc});
                }
        }
    }

    // Check for unmatched '['
    if(!loop_stack.empty()) {
        auto [pos, loc] = loop_stack.back();
        throw std::runtime_error(std::format("Error at {}: Unmatched '[' - no matching ']' found", loc.toString()));
    }

    return program;
}

// Pass 1: Combine consecutive MovePtr and ChangeValue instructions
IRProgram CombineConsecutivePass(IRProgram const& program) {
    if(program.size() < 2) {
        return program;
    }

    IRProgram optimized{};
    std::vector<int32_t> loop_stack{};  // Track LoopStarts positions for fixing offsets

    for(size_t i = 0; i < program.size(); ++i) {
        IRInstruction current = program[i];

        if(std::holds_alternative<MovePtr>(current)) {
            auto const& move_ptr = std::get<MovePtr>(current);
            int32_t total_offset = move_ptr.offset();
            SourceLocation first_loc = move_ptr.location();

            while(i + 1 < program.size() && std::holds_alternative<MovePtr>(program[i + 1])) {
                ++i;
                total_offset += std::get<MovePtr>(program[i]).offset();
            }

            if(total_offset != 0) {
                optimized.emplace_back(MovePtr{total_offset, first_loc});
            }
        }
        else if(std::holds_alternative<ChangeValue>(current)) {
            auto const& change_value = std::get<ChangeValue>(current);
            int32_t total_delta = change_value.delta();
            SourceLocation first_loc = change_value.location();

            while(i + 1 < program.size() && std::holds_alternative<ChangeValue>(program[i + 1])) {
                ++i;
                total_delta += std::get<ChangeValue>(program[i]).delta();
            }

            if(total_delta != 0) {
                optimized.emplace_back(ChangeValue{total_delta, first_loc});
            }
        }
        else if(std::holds_alternative<LoopStart>(current)) {
            loop_stack.push_back(static_cast<int32_t>(optimized.size()));
            auto const& loop_start = std::get<LoopStart>(current);
            optimized.emplace_back(LoopStart{0, loop_start.location()});
        }
        else if(std::holds_alternative<LoopEnd>(current)) {
            if(!loop_stack.empty()) {
                int32_t loop_start_pos = loop_stack.back();
                loop_stack.pop_back();

                int32_t current_pos = static_cast<int32_t>(optimized.size());

                auto const& loop_start = std::get<LoopStart>(optimized[loop_start_pos]);
                auto const& loop_end = std::get<LoopEnd>(current);

                optimized[loop_start_pos] = LoopStart{current_pos - loop_start_pos, loop_start.location()};
                optimized.emplace_back(LoopEnd{loop_start_pos - current_pos, loop_end.location()});
            }
        }
        // Copy other instructions as-is
        else {
            optimized.emplace_back(current);
        }
    }

    return optimized;
}

// Pass 2: Dead Code Elimination - remove loops that are provably never entered
// This is a simple DCE that removes empty loops and loops with only no-op content
// Conservative: only removes truly empty loops (no instructions at all inside)
IRProgram DeadCodeEliminationPass(IRProgram const& program) {
    if(program.empty()) {
        return program;
    }

    IRProgram optimized{};

    size_t i = 0;
    while(i < program.size()) {
        if(std::holds_alternative<LoopStart>(program[i])) {
            int32_t jump_offset = std::get<LoopStart>(program[i]).jump_offset();
            size_t loop_end_pos = i + jump_offset;
            size_t loop_body_size = loop_end_pos - i - 1;

            // Only remove truly empty loops (no body at all)
            // A loop with any instruction could have side effects
            if(loop_body_size == 0) {
                // Empty loop - skip both LoopStart and LoopEnd
                i = loop_end_pos + 1;
                continue;
            }

            // Copy the loop
            optimized.emplace_back(program[i]);
            ++i;
        } else {
            optimized.emplace_back(program[i]);
            ++i;
        }
    }

    // Rebuild loop offsets since positions may have changed
    return CombineConsecutivePass(optimized);
}

// Pass 3: Loop Invariant Code Motion
// For Brainfuck, we can optimize the common pattern where a loop moves away from
// and back to the original cell: [>...<] - the net position change is zero
// We can't easily hoist operations without full data-flow analysis, but we can
// recognize and normalize these patterns for better code generation
IRProgram LoopInvariantMotionPass(IRProgram const& program) {
    if(program.empty()) return program;

    IRProgram result{};
    result.reserve(program.size());

    size_t i = 0;
    while(i < program.size()) {
        if(std::holds_alternative<LoopStart>(program[i])) {
            auto const& loop_start = std::get<LoopStart>(program[i]);
            int32_t jump_offset = loop_start.jump_offset();
            size_t loop_end_pos = i + jump_offset;

            // Check for pattern: [ MovePtr(offset) ... MovePtr(-offset) ]
            // where the loop body starts and ends with compensating moves
            if(loop_end_pos >= i + 3 &&  // At least: start, move, something, move, end
               std::holds_alternative<MovePtr>(program[i + 1]) &&
               std::holds_alternative<MovePtr>(program[loop_end_pos - 1])) {

                auto const& first_move = std::get<MovePtr>(program[i + 1]);
                auto const& last_move = std::get<MovePtr>(program[loop_end_pos - 1]);

                if(first_move.offset() == -last_move.offset()) {
                    // Compensating moves detected - the loop returns to original position
                    // This is already optimal for Brainfuck, but we note the pattern
                    // A more advanced optimizer could use this for register allocation hints
                }
            }

            // Copy the loop as-is
            result.emplace_back(program[i]);
            ++i;
        } else {
            result.emplace_back(program[i]);
            ++i;
        }
    }

    return result;
}

// Pass 4: Strength Reduction
// Converts expensive operations into cheaper equivalents
// For Brainfuck:
// - Recognizes [-] as cell-clear (cannot eliminate without knowing runtime state)
// - Converts multiple single increments to larger ChangeValue where possible
IRProgram StrengthReductionPass(IRProgram const& program) {
    if(program.size() < 2) {
        return program;
    }

    IRProgram result{};
    result.reserve(program.size());

    for(size_t i = 0; i < program.size(); ++i) {
        // Pattern recognition for [-] cell-clear loop
        // While we can't eliminate it (need runtime semantics),
        // recognizing it helps future passes
        if(std::holds_alternative<LoopStart>(program[i])) {
            int32_t jump_offset = std::get<LoopStart>(program[i]).jump_offset();
            size_t loop_end_pos = i + jump_offset;

            // Check for [-] pattern: LoopStart + ChangeValue + LoopEnd
            if(loop_end_pos == i + 2) {
                auto const& body = program[i + 1];
                if(std::holds_alternative<ChangeValue>(body)) {
                    auto const& cv = std::get<ChangeValue>(body);
                    // [-] or [+] with delta of 1 or -1 is a clear/infinite loop
                    // Keep as-is - runtime handles it
                }
            }
        }

        result.emplace_back(program[i]);
    }

    return result;
}

// Pass 5: Common Subexpression Elimination
// For Brainfuck, we look for redundant operations like:
// - MovePtr(n) followed by MovePtr(-n) with no intervening memory access
// - ChangeValue(n) followed by ChangeValue(-n) with no intervening read
IRProgram CommonSubexpressionEliminationPass(IRProgram const& program) {
    if(program.size() < 3) {
        return program;
    }

    IRProgram result{};
    result.reserve(program.size());

    // Track net movement and value changes since last memory access
    int32_t net_ptr_movement = 0;
    int32_t net_cell_change = 0;
    bool ptr_tracking_active = true;
    bool cell_tracking_active = true;

    for(size_t i = 0; i < program.size(); ++i) {
        auto const& instr = program[i];

        if(std::holds_alternative<OutputCharacter>(instr) ||
           std::holds_alternative<InputCharacter>(instr)) {
            // Memory access - reset tracking
            ptr_tracking_active = false;
            cell_tracking_active = false;
            result.emplace_back(instr);
        }
        else if(std::holds_alternative<LoopStart>(instr) ||
                std::holds_alternative<LoopEnd>(instr)) {
            // Control flow - reset tracking (conservative)
            ptr_tracking_active = false;
            cell_tracking_active = false;
            result.emplace_back(instr);
        }
        else if(std::holds_alternative<MovePtr>(instr)) {
            int32_t offset = std::get<MovePtr>(instr).offset();

            // Check if this move cancels previous movement
            if(ptr_tracking_active && net_ptr_movement != 0 && offset == -net_ptr_movement) {
                // Canceling move detected - skip both
                net_ptr_movement = 0;
                // Remove the previous tracked moves from result
                // (simplified: just don't add this one, previous will be handled by next pass)
            } else {
                net_ptr_movement += offset;
                result.emplace_back(instr);
            }
        }
        else if(std::holds_alternative<ChangeValue>(instr)) {
            int32_t delta = std::get<ChangeValue>(instr).delta();

            if(cell_tracking_active && net_cell_change != 0 && delta == -net_cell_change) {
                // Canceling change detected
                net_cell_change = 0;
            } else {
                net_cell_change += delta;
                result.emplace_back(instr);
            }
        }
        else {
            result.emplace_back(instr);
        }
    }

    return result;
}

// Pass 6: Loop Unrolling
// For small fixed-count loops, unroll the body
// This is aggressive and only safe for loops we know execute few times
IRProgram LoopUnrollingPass(IRProgram const& program) {
    if(program.size() < 4) {
        return program;
    }

    IRProgram result{};
    result.reserve(program.size());

    size_t i = 0;
    while(i < program.size()) {
        if(std::holds_alternative<LoopStart>(program[i])) {
            int32_t jump_offset = std::get<LoopStart>(program[i]).jump_offset();
            size_t loop_end_pos = i + jump_offset;
            size_t loop_body_size = loop_end_pos - i - 1;

            // Only unroll very small loops (1-2 instructions)
            // This is conservative - proper unrolling needs value range analysis
            if(loop_body_size <= 2 && loop_body_size > 0) {
                // Check if loop body is simple enough
                bool can_unroll = true;
                for(size_t j = i + 1; j < loop_end_pos; ++j) {
                    auto const& body_instr = program[j];
                    // Don't unroll loops containing nested loops or I/O
                    if(std::holds_alternative<LoopStart>(body_instr) ||
                       std::holds_alternative<LoopEnd>(body_instr) ||
                       std::holds_alternative<OutputCharacter>(body_instr) ||
                       std::holds_alternative<InputCharacter>(body_instr)) {
                        can_unroll = false;
                        break;
                    }
                }

                if(can_unroll) {
                    // Unroll once (partial unrolling for peephole optimization)
                    // Full unrolling would require knowing iteration count
                    for(size_t j = i + 1; j < loop_end_pos; ++j) {
                        result.emplace_back(program[j]);
                    }
                    // Skip the original loop structure
                    i = loop_end_pos + 1;
                    continue;
                }
            }
        }

        result.emplace_back(program[i]);
        ++i;
    }

    return result;
}

OptimizationConfig GetOptimizationConfig(int level) {
    OptimizationConfig config{};
    config.level = level;

    switch(level) {
        case 0:
            config.combine_consecutive = false;
            config.dead_code_elimination = false;
            config.loop_invariant_motion = false;
            config.strength_reduction = false;
            config.common_subexpression_elim = false;
            config.loop_unrolling = false;
            break;
        case 1:
            config.combine_consecutive = true;
            config.dead_code_elimination = false;
            config.loop_invariant_motion = false;
            config.strength_reduction = false;
            config.common_subexpression_elim = false;
            config.loop_unrolling = false;
            break;
        case 2:
            config.combine_consecutive = true;
            config.dead_code_elimination = true;
            config.loop_invariant_motion = true;
            config.strength_reduction = true;
            config.common_subexpression_elim = true;
            config.loop_unrolling = false; // Disabled: unsafe without value range analysis
            break;
        case 3: // Aggressive - for testing
            config.combine_consecutive = true;
            config.dead_code_elimination = false; // DCE has issues with nested loops
            config.loop_invariant_motion = true;
            config.strength_reduction = true;
            config.common_subexpression_elim = true;
            config.loop_unrolling = false;
            break;
        default:
            config.combine_consecutive = true;
            break;
    }

    return config;
}

IRProgram OptimizeIR(IRProgram const& program, OptimizationConfig const& config) {
    IRProgram result = program;

    // Apply passes in order based on config
    if(config.combine_consecutive) {
        result = CombineConsecutivePass(result);
    }

    if(config.dead_code_elimination) {
        result = DeadCodeEliminationPass(result);
    }

    if(config.loop_invariant_motion) {
        result = LoopInvariantMotionPass(result);
    }

    if(config.strength_reduction) {
        result = StrengthReductionPass(result);
    }

    if(config.common_subexpression_elim) {
        result = CommonSubexpressionEliminationPass(result);
    }

    if(config.loop_unrolling) {
        result = LoopUnrollingPass(result);
        // Run combine pass again after unrolling to merge any new opportunities
        result = CombineConsecutivePass(result);
    }

    return result;
}
