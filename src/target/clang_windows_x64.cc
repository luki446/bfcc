#include "clang_windows_x64.hpp"

void ClangWindowsX64Target::compile(IRProgram const& program, std::filesystem::path const& outputPath) {
    
}

static RegisterTarget<ClangWindowsX64Target> registerClangWindowsX64Target{"x86_64-clang-windows"};