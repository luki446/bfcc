#pragma once

#include "../compiler.hpp"

class CTarget : public Target {
public:
    std::string getName() const override { return "portable-c"; }
    std::string getDescription() const override { return "Generates portable C source code"; }
    TargetCapability getCapabilities() const override {
        return TargetCapability::EmitsAssembly; // We use EmitsAssembly for source code output
    }

    CompilationResult compile(IRProgram const& program, std::filesystem::path const& outputPath) override;
    CompilationResult generateAssembly(IRProgram const& program) override;

private:
    std::string generateCCode(IRProgram const& program);
    void doCompile(IRProgram const& program, std::filesystem::path const& outputPath, CompilationResult& result) override;
    void doGenerateAssembly(IRProgram const& program, CompilationResult& result) override;
};
