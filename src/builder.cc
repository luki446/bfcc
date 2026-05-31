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

// Pass 2: Clear Cell Detection
// Replaces [-] and [+] loops (single ChangeValue{-1} or {+1} body) with ClearCell
// These are the most common Brainfuck patterns for setting a cell to zero
IRProgram ClearCellPass(IRProgram const& program) {
    if(program.size() < 3) {
        return program;
    }

    IRProgram result{};
    result.reserve(program.size());

    size_t i = 0;
    while(i < program.size()) {
        if(std::holds_alternative<LoopStart>(program[i])) {
            auto const& loop_start = std::get<LoopStart>(program[i]);
            int32_t jump_offset = loop_start.jump_offset();
            size_t loop_end_pos = i + jump_offset;
            size_t loop_body_size = loop_end_pos - i - 1;

            // Check for [-] pattern: LoopStart, single ChangeValue{-1 or +1}, LoopEnd
            if(loop_body_size == 1) {
                auto const& body = program[i + 1];
                if(std::holds_alternative<ChangeValue>(body)) {
                    auto const& cv = std::get<ChangeValue>(body);
                    if(cv.delta() == -1 || cv.delta() == 1) {
                        // This is a [-] or [+] clear-cell loop
                        result.emplace_back(ClearCell{cv.location()});
                        // Skip LoopStart, body, and LoopEnd
                        i = loop_end_pos + 1;
                        continue;
                    }
                }
            }

            result.emplace_back(program[i]);
            ++i;
        } else {
            result.emplace_back(program[i]);
            ++i;
        }
    }

    return result;
}

// Pass 3: Multiplication Loop Detection
// Recognizes patterns like [->++<] which compute:
//   while cell[0] != 0:
//     cell[0] -= 1
//     cell[offset] += multiplier
//   cell[0] = 0
//
// This replaces the entire loop with a Multiplication instruction
IRProgram MultiplicationLoopPass(IRProgram const& program) {
    if(program.size() < 5) {
        return program;
    }

    IRProgram result{};
    result.reserve(program.size());

    size_t i = 0;
    while(i < program.size()) {
        if(!std::holds_alternative<LoopStart>(program[i])) {
            result.emplace_back(program[i]);
            ++i;
            continue;
        }

        auto const& loop_start = std::get<LoopStart>(program[i]);
        int32_t jump_offset = loop_start.jump_offset();
        size_t loop_end_pos = i + jump_offset;
        size_t loop_body_size = loop_end_pos - i - 1;

        // Minimum body: ChangeValue{-1}, MovePtr{n}, ChangeValue{m}, MovePtr{-n}
        // Or: MovePtr{n}, ChangeValue{m}, MovePtr{-n}, ChangeValue{-1}
        if(loop_body_size < 2 || loop_body_size > 8) {
            result.emplace_back(program[i]);
            ++i;
            continue;
        }

        // Analyze the loop body
        int32_t current_offset = 0;
        bool has_decrement = false;
        bool net_zero_offset = true;
        std::vector<std::pair<int32_t, int32_t>> cell_deltas; // (offset, delta)
        bool is_multiplication = true;

        for(size_t j = i + 1; j < loop_end_pos && is_multiplication; ++j) {
            auto const& instr = program[j];

            if(std::holds_alternative<MovePtr>(instr)) {
                current_offset += std::get<MovePtr>(instr).offset();
            }
            else if(std::holds_alternative<ChangeValue>(instr)) {
                auto const& cv = std::get<ChangeValue>(instr);
                if(current_offset == 0 && cv.delta() == -1) {
                    // Found the decrement on the current cell
                    if(has_decrement) {
                        // Multiple decrements - not a simple multiplication
                        is_multiplication = false;
                    } else {
                        has_decrement = true;
                    }
                } else {
                    // Record change at this offset
                    cell_deltas.emplace_back(current_offset, cv.delta());
                }
            }
            else if(std::holds_alternative<LoopStart>(instr) ||
                    std::holds_alternative<LoopEnd>(instr)) {
                // Nested loops - not a simple multiplication
                is_multiplication = false;
            }
            else {
                // I/O in loop body - not a multiplication
                is_multiplication = false;
            }
        }

        // Verify the pattern
        if(!is_multiplication || !has_decrement || current_offset != 0) {
            result.emplace_back(program[i]);
            ++i;
            continue;
        }

        // Must have exactly one target cell with a non-zero delta
        // (Multiple targets are possible but complex to represent)
        if(cell_deltas.size() == 1) {
            auto [offset, delta] = cell_deltas[0];
            auto loc = loop_start.location();
            result.emplace_back(Multiplication{offset, delta, loc});
            i = loop_end_pos + 1;
            continue;
        }

        result.emplace_back(program[i]);
        ++i;
    }

    return result;
}

// Pass 4: Common Subexpression Elimination
// For Brainfuck, we look for redundant operations like:
// - MovePtr(n) followed by MovePtr(-n) with no intervening memory access
// - ChangeValue(n) followed by ChangeValue(-n) with no intervening read
// This is a peephole optimizer that scans backwards
IRProgram CommonSubexpressionEliminationPass(IRProgram const& program) {
    if(program.size() < 2) {
        return program;
    }

    IRProgram result = program;
    bool changed = true;

    // Run multiple passes until no more changes
    while(changed) {
        changed = false;
        IRProgram pass_result{};

        for(size_t i = 0; i < result.size(); ++i) {
            // Look for canceling pair: MovePtr(n) followed immediately by MovePtr(-n)
            if(i + 1 < result.size() &&
               std::holds_alternative<MovePtr>(result[i]) &&
               std::holds_alternative<MovePtr>(result[i + 1])) {
                auto const& first = std::get<MovePtr>(result[i]);
                auto const& second = std::get<MovePtr>(result[i + 1]);

                if(first.offset() == -second.offset()) {
                    // Canceling moves - skip both
                    changed = true;
                    i += 2;
                    continue;
                }
            }

            // Look for canceling pair: ChangeValue(n) followed immediately by ChangeValue(-n)
            if(i + 1 < result.size() &&
               std::holds_alternative<ChangeValue>(result[i]) &&
               std::holds_alternative<ChangeValue>(result[i + 1])) {
                auto const& first = std::get<ChangeValue>(result[i]);
                auto const& second = std::get<ChangeValue>(result[i + 1]);

                if(first.delta() == -second.delta()) {
                    // Canceling changes - skip both
                    changed = true;
                    i += 2;
                    continue;
                }
            }

            pass_result.emplace_back(result[i]);
        }

        result = std::move(pass_result);
    }

    // Rebuild loop offsets since we may have removed instructions
    return CombineConsecutivePass(result);
}

OptimizationConfig GetOptimizationConfig(int level) {
    OptimizationConfig config{};
    config.level = level;

    switch(level) {
        case 0:
            config.combine_consecutive = false;
            config.clear_cell = false;
            config.multiplication_loop = false;
            config.common_subexpression_elim = false;
            break;
        case 1:
            config.combine_consecutive = true;
            config.clear_cell = true;
            config.multiplication_loop = false;
            config.common_subexpression_elim = false;
            break;
        case 2:
            config.combine_consecutive = true;
            config.clear_cell = true;
            config.multiplication_loop = true;
            config.common_subexpression_elim = true;
            break;
        case 3: // Aggressive
            config.combine_consecutive = true;
            config.clear_cell = true;
            config.multiplication_loop = true;
            config.common_subexpression_elim = true;
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
    // Order matters: detect patterns first, then eliminate redundancies
    if(config.clear_cell) {
        result = ClearCellPass(result);
    }

    if(config.multiplication_loop) {
        result = MultiplicationLoopPass(result);
    }

    if(config.combine_consecutive) {
        result = CombineConsecutivePass(result);
    }

    if(config.common_subexpression_elim) {
        result = CommonSubexpressionEliminationPass(result);
    }

    return result;
}
