#include <CLI11.hpp>
#include "utils.hpp"
#include "runner.hpp"

int main(int argc, char** argv) {
    CLI::App app{"bfcc - Brainfuck (Overly)Complicated Compiler"};
    app.require_subcommand(1);

    auto run_command = app.add_subcommand("run", "Run a Brainfuck program in interpreter mode");

    std::filesystem::path input_file{};
    run_command->add_option("input_file", input_file, "Path to the Brainfuck source file");

    CLI11_PARSE(app, argc, argv);

    if(*run_command) {
        auto content = ReadContentFromFile(input_file);
        RunProgram(content);
    }

    return 0;
}