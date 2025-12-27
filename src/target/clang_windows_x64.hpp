#pragma once 

#include "../compiler.hpp"

class ClangWindowsX64Target : public Target {
public:
    void compile(IRProgram const& program, std::filesystem::path const& outputPath) override;
};