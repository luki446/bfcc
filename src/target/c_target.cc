#include "c_target.hpp"
#include <fstream>

struct CCodeVisitor {
    std::string operator()(MovePtr const& move_ptr) {
        int32_t offset = move_ptr.offset();
        if(offset > 0) {
            return std::format("    ptr += {};\n", offset);
        } else if(offset < 0) {
            return std::format("    ptr -= {};\n", -offset);
        }
        return "";
    }

    std::string operator()(ChangeValue const& change_value) {
        int32_t delta = change_value.delta();
        if(delta > 0) {
            return std::format("    tape[ptr] += {};\n", delta);
        } else if(delta < 0) {
            return std::format("    tape[ptr] -= {};\n", -delta);
        }
        return "";
    }

    std::string operator()(OutputCharacter const&) {
        return "    putchar(tape[ptr]);\n";
    }

    std::string operator()(InputCharacter const&) {
        return "    tape[ptr] = getchar();\n";
    }

    std::string operator()(LoopStart const&) {
        return "    while(tape[ptr]) {\n";
    }

    std::string operator()(LoopEnd const&) {
        return "    }\n";
    }

    std::string operator()(ClearCell const&) {
        return "    tape[ptr] = 0;\n";
    }

    std::string operator()(Multiplication const& mult) {
        int32_t offset = mult.offset();
        int32_t multiplier = mult.multiplier();

        if(offset >= 0) {
            if(multiplier == 1) {
                return std::format("    tape[ptr + {}] += tape[ptr];\n    tape[ptr] = 0;\n", offset);
            } else if(multiplier == -1) {
                return std::format("    tape[ptr + {}] -= tape[ptr];\n    tape[ptr] = 0;\n", offset);
            } else if(multiplier > 0) {
                return std::format("    tape[ptr + {}] += tape[ptr] * {};\n    tape[ptr] = 0;\n", offset, multiplier);
            } else {
                return std::format("    tape[ptr + {}] -= tape[ptr] * {};\n    tape[ptr] = 0;\n", offset, -multiplier);
            }
        } else {
            if(multiplier == 1) {
                return std::format("    tape[ptr - {}] += tape[ptr];\n    tape[ptr] = 0;\n", -offset);
            } else if(multiplier == -1) {
                return std::format("    tape[ptr - {}] -= tape[ptr];\n    tape[ptr] = 0;\n", -offset);
            } else if(multiplier > 0) {
                return std::format("    tape[ptr - {}] += tape[ptr] * {};\n    tape[ptr] = 0;\n", -offset, multiplier);
            } else {
                return std::format("    tape[ptr - {}] -= tape[ptr] * {};\n    tape[ptr] = 0;\n", -offset, -multiplier);
            }
        }
    }
};

std::string CTarget::generateCCode(IRProgram const& program) {
    std::string code{};

    code += R"(#include <stdio.h>
#include <stdint.h>

#define TAPE_SIZE 30000

int main(void) {
    uint8_t tape[TAPE_SIZE] = {0};
    int ptr = 0;

)";

    for(size_t i = 0; i < program.size(); ++i) {
        code += std::visit(CCodeVisitor{}, program[i]);
    }

    code += R"(
    return 0;
}
)";

    return code;
}

void CTarget::doGenerateAssembly(IRProgram const& program, CompilationResult& result) {
    result.generatedCode = generateCCode(program);
    result.success = true;
}

CompilationResult CTarget::generateAssembly(IRProgram const& program) {
    CompilationResult result;
    doGenerateAssembly(program, result);
    return result;
}

void CTarget::doCompile(IRProgram const& program, std::filesystem::path const& outputPath, CompilationResult& result) {
    std::string c_code = generateCCode(program);

    std::filesystem::path output_file = outputPath;
    if(output_file.extension() != ".c") {
        output_file.replace_extension(".c");
    }

    // Write C source file
    std::ofstream file(output_file);
    if(!file) {
        result.success = false;
        result.errorMessage = "Failed to open output file for writing";
        return;
    }

    file << c_code;
    file.close();

    result.outputPath = output_file;
    result.success = true;
}

CompilationResult CTarget::compile(IRProgram const& program, std::filesystem::path const& outputPath) {
    CompilationResult result;
    doCompile(program, outputPath, result);
    return result;
}

static RegisterTarget<CTarget> registerCTarget{"portable-c", "Generates portable C source code"};
