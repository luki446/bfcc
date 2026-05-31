#include "clang_windows_x64.hpp"

#include <windows.h>
#include <stdio.h>

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

    std::string operator()(ClearCell const&) {
        // ClearCell: mov byte ptr [rbx], 0
        return "\tmov byte ptr [rbx], 0\n";
    }

    std::string operator()(Multiplication const& mult) {
        // Multiplication: cell[offset] += cell[0] * multiplier, then cell[0] = 0
        // For multiplier == 1:  add [rbx+offset], [rbx]; mov [rbx], 0
        // For multiplier == -1: sub [rbx+offset], [rbx]; mov [rbx], 0
        // For other values: use imul
        std::string code{};

        if(mult.multiplier() == 1) {
            // Add source to destination
            code += std::format("\tmovzx rax, byte ptr [rbx]\n");
            code += std::format("\tadd byte ptr [rbx + {}], al\n", mult.offset());
            code += "\tmov byte ptr [rbx], 0\n";
        } else if(mult.multiplier() == -1) {
            // Subtract source from destination
            code += std::format("\tmovzx rax, byte ptr [rbx]\n");
            code += std::format("\tsub byte ptr [rbx + {}], al\n", mult.offset());
            code += "\tmov byte ptr [rbx], 0\n";
        } else {
            // General case with imul
            code += std::format("\tmovzx rax, byte ptr [rbx]\n");
            code += std::format("\timul rax, {}\n", mult.multiplier());
            code += std::format("\tadd byte ptr [rbx + {}], al\n", mult.offset());
            code += "\tmov byte ptr [rbx], 0\n";
        }

        return code;
    }

    int32_t ir_instruction_index_;
};

std::string ClangWindowsX64Target::doGenerateAssembly(IRProgram const& program) {
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

void ClangWindowsX64Target::doGenerateAssembly(IRProgram const& program, CompilationResult& result) {
    result.generatedCode = doGenerateAssembly(program);
    result.success = true;
}

CompilationResult ClangWindowsX64Target::generateAssembly(IRProgram const& program) {
    CompilationResult result;
    doGenerateAssembly(program, result);
    return result;
}

void ClangWindowsX64Target::doCompile(IRProgram const& program, std::filesystem::path const& outputPath, CompilationResult& result) {
    std::string asm_code = doGenerateAssembly(program);

    std::filesystem::path exe_path = outputPath;
    exe_path.replace_extension(".exe");
    result.outputPath = exe_path;

    // Find clang executable
    char clang_path_buf[MAX_PATH];
    DWORD searchResult = SearchPathA(nullptr, "clang.exe", nullptr, MAX_PATH, clang_path_buf, nullptr);

    if (searchResult == 0) {
        result.success = false;
        result.errorMessage = "Clang not found in PATH";
        return;
    }

    // Create pipes for communication with clang process
    HANDLE hPipeRead = nullptr;
    HANDLE hPipeWrite = nullptr;

    SECURITY_ATTRIBUTES saAttr{};
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = nullptr;

    if (!CreatePipe(&hPipeRead, &hPipeWrite, &saAttr, 0)) {
        result.success = false;
        result.errorMessage = "Failed to create pipe";
        return;
    }

    // Ensure the write handle is not inherited
    if (!SetHandleInformation(hPipeWrite, HANDLE_FLAG_INHERIT, 0)) {
        CloseHandle(hPipeRead);
        CloseHandle(hPipeWrite);
        result.success = false;
        result.errorMessage = "Failed to set pipe handle information";
        return;
    }

    // Prepare startup info
    STARTUPINFOA siStartInfo{};
    siStartInfo.cb = sizeof(STARTUPINFOA);
    siStartInfo.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    siStartInfo.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    siStartInfo.hStdInput = hPipeRead;
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

    // Build command line
    std::string cmdLine = std::string("\"") + clang_path_buf + "\" -x assembler -o " + exe_path.string() + " -";

    // Create process
    PROCESS_INFORMATION piProcInfo{};

    BOOL bSuccess = CreateProcessA(
        nullptr,                    // Application name
        cmdLine.data(),             // Command line
        nullptr,                    // Process security attributes
        nullptr,                    // Thread security attributes
        TRUE,                       // Inherit handles
        0,                          // Creation flags
        nullptr,                    // Environment block
        nullptr,                    // Current directory
        &siStartInfo,               // Startup info
        &piProcInfo                 // Process info
    );

    CloseHandle(hPipeRead);

    if (!bSuccess) {
        CloseHandle(hPipeWrite);
        result.success = false;
        result.errorMessage = "Failed to create clang process";
        return;
    }

    // Write assembly code to clang's stdin
    DWORD bytesWritten = 0;
    if (!WriteFile(hPipeWrite, asm_code.data(), static_cast<DWORD>(asm_code.size()), &bytesWritten, nullptr)) {
        CloseHandle(hPipeWrite);
        CloseHandle(piProcInfo.hProcess);
        CloseHandle(piProcInfo.hThread);
        result.success = false;
        result.errorMessage = "Failed to write assembly code to clang";
        return;
    }

    CloseHandle(hPipeWrite);

    // Wait for clang to finish
    WaitForSingleObject(piProcInfo.hProcess, INFINITE);

    // Get exit code
    DWORD exitCode = 0;
    if (!GetExitCodeProcess(piProcInfo.hProcess, &exitCode)) {
        CloseHandle(piProcInfo.hProcess);
        CloseHandle(piProcInfo.hThread);
        result.success = false;
        result.errorMessage = "Failed to get clang exit code";
        return;
    }

    CloseHandle(piProcInfo.hProcess);
    CloseHandle(piProcInfo.hThread);

    if (exitCode != 0) {
        result.success = false;
        result.errorMessage = "Clang failed to compile assembly code";
        return;
    }

    result.success = true;
}

CompilationResult ClangWindowsX64Target::compile(IRProgram const& program, std::filesystem::path const& outputPath) {
    CompilationResult result;
    doCompile(program, outputPath, result);
    return result;
}

static RegisterTarget<ClangWindowsX64Target> registerClangWindowsX64Target{"x86_64-clang-windows", "Windows x86_64 using Clang assembler"};
