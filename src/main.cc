#include <CLI11.hpp>
#include <print>
#include "utils.hpp"
#include "runner.hpp"
#include "builder.hpp"
#include "compiler.hpp"

int main(int argc, char** argv) {
    // Handle --version before parsing (allows it without subcommand)
    for(int i = 1; i < argc; ++i) {
        if(std::string(argv[i]) == "--version") {
            std::println("bfcc v0.1.0 - Brainfuck (Overly)Complicated Compiler");
            return 0;
        }
    }

    CLI::App app{"bfcc - Brainfuck (Overly)Complicated Compiler"};
    app.require_subcommand(1);

    auto run_command = app.add_subcommand("run", "Run a Brainfuck program in interpreter mode");

    std::string input_file{};
    run_command->add_option("input_file", input_file, "Path to the Brainfuck source file (reads from stdin if not provided)");

    // Run command options
    auto tape_size_opt = run_command->add_option("--tape-size", "Size of the memory tape in bytes")->default_str("30720");
    auto cell_size_opt = run_command->add_option("--cell-size", "Cell size in bits (8, 16, or 32)")->default_str("8")->check(CLI::IsMember({"8", "16", "32"}));
    auto bounds_check = run_command->add_flag("--bounds-check", "Enable runtime bounds checking");
    auto overflow_wrap = run_command->add_flag("--overflow-saturate", "Use saturation arithmetic on overflow (default: wrap)");

    auto build_command = app.add_subcommand("build", "Build a Brainfuck program");
    build_command->add_option("input_file", input_file, "Path to the Brainfuck source file (reads from stdin if not provided)");
    auto output_file = build_command->add_option("-o,--output", "Path to the output executable file")->default_str("a.out");

    auto available_targets = TargetRegistry::get().getTargetsList();
    auto target_option = build_command->add_option("--target", "Target to build for")->default_str("x86_64-clang-windows")->check(CLI::IsMember(available_targets));

    auto list_targets = build_command->add_flag("--list-targets", "List all available targets");
    auto emit_ir = build_command->add_flag("--emit-ir", "Emit the generated IR to stdout");
    auto emit_asm = build_command->add_flag("--emit-asm", "Emit the generated assembly to stdout");
    auto optimize = build_command->add_flag("-O,--optimize", "Optimize the generated code");
    auto optimize_level = build_command->add_option("--optimize-level", "Optimization level (0=none, 1=basic)")->default_str("1");
    auto verbose = build_command->add_flag("-v,--verbose", "Enable verbose output");

    CLI11_PARSE(app, argc, argv);

    std::string content;
    if(!input_file.empty()) {
        content = ReadContentFromFile(input_file);
    } else {
        content = ReadContentFromStdin();
    }

    if(*run_command) {
        RunConfig config{};
        config.tape_size = tape_size_opt->as<size_t>();
        config.cell_size = cell_size_opt->as<size_t>();
        config.bounds_check = bounds_check->as<bool>();
        config.overflow_wrap = !overflow_wrap->as<bool>();

        RunProgram(content, config);
    } else if(*build_command) {
        if(*list_targets) {
            auto const& registry = TargetRegistry::get();
            for(auto const& target_name : available_targets) {
                std::println("{} - {}", target_name, registry.getTargetDescription(target_name));
            }
            return 0;
        }

        IRProgram program = BuildIR(content);

        // Build optimization config from flags
        OptimizationConfig opt_config{};
        int opt_level = optimize_level->as<int>();

        if(opt_level == 0 || (*optimize && opt_level < 1)) {
            opt_config = GetOptimizationConfig(0);
        } else if(opt_level == 2) {
            opt_config = GetOptimizationConfig(2);
        } else {
            opt_config = GetOptimizationConfig(1); // Default
        }

        if(*verbose && (opt_config.combine_consecutive || opt_config.dead_code_elimination)) {
            std::println("Applying optimization level {}...", opt_level);
        }

        program = OptimizeIR(program, opt_config);

        auto target = TargetRegistry::get().createTarget(target_option->as<std::string>());
        if(!target) {
            std::println("Error: Unknown target '{}'", target_option->as<std::string>());
            return 1;
        }

        if(*emit_ir) {
            if(*verbose) {
                std::println("{:=^40}", " IR Dump ");
                std::println("Generated IR ({} instructions):", program.size());
                std::println("{:=^40}", "");
            }
            std::println("{}", program);
        } else if(*emit_asm) {
            if(*verbose) {
                std::println("{:=^40}", " Assembly Generation ");
                std::println("Generating assembly for target: {}", target_option->as<std::string>());
                std::println("{:=^40}", "");
            }

            auto result = target->generateAssembly(program);
            if(!result.success) {
                std::println("Error: {}", result.errorMessage);
                return 1;
            }
            std::print("{}", result.generatedCode);
        } else { // emit executable
            if(*verbose) {
                std::println("{:=^40}", " Build Info ");
                std::println("Building program with {} instructions", program.size());
                std::println("Target: {}", target_option->as<std::string>());
                std::println("{:=^40}", "");
            }

            std::filesystem::path output_path = output_file->as<std::string>();
            auto result = target->compile(program, output_path);

            if(!result.success) {
                std::println("Build failed: {}", result.errorMessage);
                return 1;
            }

            if(*verbose) {
                std::println("Output: {}", result.outputPath.string());
            }
        }
    }
    return 0;
}