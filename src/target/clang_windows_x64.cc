#include "clang_windows_x64.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/writable_pipe.hpp>
#include <boost/process.hpp>
#include <boost/asio.hpp>
#include <boost/process/v2/environment.hpp>
#include <boost/process/v2/process.hpp>
#include <boost/process/v2/stdio.hpp>

struct X64AsmVisitor {
    X64AsmVisitor(int32_t instruction_index) : ir_instruction_index_(instruction_index) {}

    // RBX - data pointer

    std::string operator()(MovePtr const& move_ptr) {
        return std::format("\tadd rbx, {}\n", move_ptr.offset());
    }

    std::string operator()(ChangeValue const& change_value) {
        return std::format("\tadd byte ptr [rbx], {}\n", change_value.delta());
    }

    std::string operator()(OutputCharacter const&) {
        std::string code{};
        code += "\tmovzx rcx, byte ptr [rbx]\n";
        code += "\tcall putchar\n";
        return code;
    }

    std::string operator()(InputCharacter const&) {
        std::string code{};
        code += "\tcall getchar\n";
        code += "\tmov byte ptr [rbx], al\n";
        return code;
    }

    std::string operator()(LoopStart const& loop_start) {
        std::string code{};

        code += std::format("loop_start_{}:\n", ir_instruction_index_);
        code += "\tcmp byte ptr [rbx], 0\n";
        code += std::format("\tje loop_end_{}\n", loop_start.jump_offset() + ir_instruction_index_);

        return code;
    }

    std::string operator()(LoopEnd const& loop_end) {
        std::string code{};

        code += std::format("\tjmp loop_start_{}\n", loop_end.jump_offset() + ir_instruction_index_);
        code += std::format("loop_end_{}:\n", ir_instruction_index_);

        return code;
    }

    int32_t ir_instruction_index_;
};

std::string ClangWindowsX64Target::generateAssembly(IRProgram const& program) {
    std::string asm_code{};

    asm_code += R"(
.intel_syntax noprefix

.extern putchar
.extern getchar

.section .bss
tape:
    .skip 30000

.section .text
.globl main

main:
    push rbx

    sub rsp, 32
    
    lea rbx, tape[rip]
)"; // Standard header and 32-byte shadow space + 16-byte alignment and tape allocation
    
    for(int32_t i = 0; i < program.size(); ++i) {
        asm_code += std::visit(X64AsmVisitor{i}, program[i]);
    }

    asm_code += R"(

    xor eax, eax
    add rsp, 32
    pop rbx
    ret)"; // Standard footer

    return asm_code;
}

void ClangWindowsX64Target::compile(IRProgram const& program, std::filesystem::path const& outputPath) {
    std::string asm_code = generateAssembly(program);

    std::filesystem::path exe_path = outputPath;;
    exe_path.replace_extension(".exe");

    boost::asio::io_context ctx{};
    boost::asio::writable_pipe writable_pipe{ctx};

    auto clang_path = boost::process::v2::environment::find_executable("clang");

    if(clang_path.empty()) {
        throw std::runtime_error("Clang not found in PATH");
    }

    boost::process::process proc(ctx, clang_path, 
        {"-x", "assembler", 
        "-o", exe_path.string(), 
        "-"},
        boost::process::process_stdio{
            writable_pipe,
            {},
            {}
        }
    );

    boost::asio::write(writable_pipe, boost::asio::buffer(asm_code));
    writable_pipe.close();

    proc.wait();

    if(proc.exit_code() != 0) {
        throw std::runtime_error("Clang failed to compile assembly code");
    }
}

static RegisterTarget<ClangWindowsX64Target> registerClangWindowsX64Target{"x86_64-clang-windows"};