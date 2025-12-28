#include <CLI11.hpp>
#include <print>
#include "utils.hpp"
#include "runner.hpp"
#include "builder.hpp"
#include "compiler.hpp"

int main(int argc, char** argv) {
    CLI::App app{"bfcc - Brainfuck (Overly)Complicated Compiler"};
    app.require_subcommand(1);

    auto run_command = app.add_subcommand("run", "Run a Brainfuck program in interpreter mode");

    std::string input_file{};
    run_command->add_option("input_file", input_file, "Path to the Brainfuck source file (reads from stdin if not provided)");

    auto build_command = app.add_subcommand("build", "Build a Brainfuck program");
    build_command->add_option("input_file", input_file, "Path to the Brainfuck source file (reads from stdin if not provided)");
    auto output_file = build_command->add_option("-o,--output", "Path to the output executable file")->default_str("a.out");

    auto available_targets = TargetRegistry::get().getTargetsList();
    auto target_option = build_command->add_option("--target", "Target to build for")->default_str("x86_64-clang-windows")->check(CLI::IsMember(available_targets));

    auto list_targets = build_command->add_flag("--list-targets", "List all available targets");
    auto emit_ir = build_command->add_flag("--emit-ir", "Emit the generated IR to stdout");
    auto optimize = build_command->add_flag("-O,--optimize", "Optimize the generated code");
    auto verbose = build_command->add_flag("-v,--verbose", "Enable verbose output");

    CLI11_PARSE(app, argc, argv);

    std::string content;
    if(!input_file.empty()) {
        content = ReadContentFromFile(input_file);
    } else {
        content = ReadContentFromStdin();
    }

    if(*run_command) {
        RunProgram(content);
    } else if(*build_command) {
        if(*list_targets) {
            for(auto const& target_name : available_targets) {
                std::println("{}", target_name);
            }
            return 0;
        }

        IRProgram program = BuildIR(content);

        // Here optimize IR
        if(*optimize)
            program = OptimizeIR(program);

        if(*emit_ir) {
            if(*verbose) {
                std::println("{:=^40}", " IR Dump ");
                std::println("Generated IR ({} instructions):", program.size());
                std::println("{:=^40}", "");
            }
            std::println("{}", program);
        } else { // emit executable
            if(*verbose) {
                std::println("{:=^40}", " Build Info ");
                std::println("Building program with {} instructions", program.size());
                std::println("{:=^40}", "");
            }

            auto target = TargetRegistry::get().createTarget(target_option->as<std::string>());

            std::filesystem::path output_path = output_file->as<std::string>();
            target->compile(program, output_path);
        }   
    }
    return 0;
}