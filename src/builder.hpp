#pragma once

#include "ir.hpp"
#include <string>
#include <functional>

// Optimization pass type - takes a program and returns optimized version
using OptimizationPass = std::function<IRProgram(IRProgram const&)>;

// Optimization level configuration
struct OptimizationConfig {
    int level = 1;                          // 0=none, 1=basic, 2=advanced
    bool combine_consecutive = true;        // Combine consecutive MovePtr/ChangeValue
    bool clear_cell = false;                // Replace [-] with ClearCell instruction
    bool multiplication_loop = false;         // Detect and replace multiplication loops
    bool common_subexpression_elim = false; // Eliminate canceling operations
};

// Build IR from source
IRProgram BuildIR(std::string const& source);

// Apply all enabled optimizations based on level
IRProgram OptimizeIR(IRProgram const& program, OptimizationConfig const& config = {});

// Individual optimization passes
IRProgram CombineConsecutivePass(IRProgram const& program);
IRProgram ClearCellPass(IRProgram const& program);
IRProgram MultiplicationLoopPass(IRProgram const& program);
IRProgram CommonSubexpressionEliminationPass(IRProgram const& program);

// Get default config for optimization level
OptimizationConfig GetOptimizationConfig(int level);
