#pragma once

#include "../compiler.hpp"

class ClangWindowsX64Target : public Target {
public:
    std::string getName() const override { return "x86_64-clang-windows"; }
    std::string getDescription() const override { return "Windows x86_64 using Clang assembler"; }
    TargetCapability getCapabilities() const override {
        return TargetCapability::EmitsAssembly | TargetCapability::EmitsExecutable;
    }

    CompilationResult compile(IRProgram const& program, std::filesystem::path const& outputPath) override;
    CompilationResult generateAssembly(IRProgram const& program) override;

private:
    std::string doGenerateAssembly(IRProgram const& program);
    void doCompile(IRProgram const& program, std::filesystem::path const& outputPath, CompilationResult& result) override;
    void doGenerateAssembly(IRProgram const& program, CompilationResult& result) override;
};
