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
    bool dead_code_elimination = false;     // Remove provably dead code
    bool loop_invariant_motion = false;     // Move invariant code out of loops
    bool strength_reduction = false;        // Convert operations to cheaper equivalents
    bool common_subexpression_elim = false; // Eliminate redundant operations
    bool loop_unrolling = false;            // Unroll small fixed-count loops
};

// Build IR from source
IRProgram BuildIR(std::string const& source);

// Apply all enabled optimizations based on level
IRProgram OptimizeIR(IRProgram const& program, OptimizationConfig const& config = {});

// Individual optimization passes
IRProgram CombineConsecutivePass(IRProgram const& program);
IRProgram DeadCodeEliminationPass(IRProgram const& program);
IRProgram LoopInvariantMotionPass(IRProgram const& program);
IRProgram StrengthReductionPass(IRProgram const& program);
IRProgram CommonSubexpressionEliminationPass(IRProgram const& program);
IRProgram LoopUnrollingPass(IRProgram const& program);

// Get default config for optimization level
OptimizationConfig GetOptimizationConfig(int level);
