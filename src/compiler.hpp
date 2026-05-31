#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "ir.hpp"

// Compilation result containing output path or generated code
struct CompilationResult {
    bool success = true;
    std::string errorMessage;
    std::filesystem::path outputPath;
    std::string generatedCode; // For --emit-asm or --emit-ir
};

// Target capabilities bitfield
enum class TargetCapability : uint32_t {
    None = 0,
    EmitsAssembly = 1 << 0,
    EmitsObject = 1 << 1,
    EmitsExecutable = 1 << 2,
};

inline TargetCapability operator|(TargetCapability a, TargetCapability b) {
    return static_cast<TargetCapability>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline uint32_t operator&(TargetCapability a, TargetCapability b) {
    return static_cast<uint32_t>(a) & static_cast<uint32_t>(b);
}

class Target {
public:
    // Get target description
    virtual std::string getName() const { return "unknown"; }
    virtual std::string getDescription() const { return "Unknown target"; }
    virtual TargetCapability getCapabilities() const { return TargetCapability::EmitsExecutable; }

    // Primary compile method - produces executable
    virtual CompilationResult compile(IRProgram const& program, std::filesystem::path const& outputPath) {
        CompilationResult result;
        result.outputPath = outputPath;

        if (!(getCapabilities() & TargetCapability::EmitsExecutable)) {
            result.success = false;
            result.errorMessage = "Target does not support executable output";
            return result;
        }

        doCompile(program, outputPath, result);
        return result;
    }

    // Generate assembly code (for --emit-asm)
    virtual CompilationResult generateAssembly(IRProgram const& program) {
        CompilationResult result;

        if (!(getCapabilities() & TargetCapability::EmitsAssembly)) {
            result.success = false;
            result.errorMessage = "Target does not support assembly output";
            return result;
        }

        doGenerateAssembly(program, result);
        return result;
    }

    // Generate object file (for --emit-obj)
    virtual CompilationResult generateObject(IRProgram const& program, std::filesystem::path const& outputPath) {
        CompilationResult result;
        result.outputPath = outputPath;

        if (!(getCapabilities() & TargetCapability::EmitsObject)) {
            result.success = false;
            result.errorMessage = "Target does not support object file output";
            return result;
        }

        doGenerateObject(program, outputPath, result);
        return result;
    }

    virtual ~Target() = default;

protected:
    // Implementation methods for derived classes
    virtual void doCompile(IRProgram const& program, std::filesystem::path const& outputPath, CompilationResult& result) = 0;
    virtual void doGenerateAssembly(IRProgram const& program, CompilationResult& result) {
        result.success = false;
        result.errorMessage = "Assembly generation not implemented for this target";
    }
    virtual void doGenerateObject(IRProgram const& program, std::filesystem::path const& outputPath, CompilationResult& result) {
        result.success = false;
        result.errorMessage = "Object file generation not implemented for this target";
    }
};

using TargetFactory = std::function<std::unique_ptr<Target>()>;

class TargetRegistry {
public:
    static TargetRegistry& get();
    void registerTarget(std::string const& name, TargetFactory factory, std::string description = "");
    std::vector<std::string> getTargetsList() const;
    std::unique_ptr<Target> createTarget(std::string const& name);
    std::string getTargetDescription(std::string const& name) const;
private:
    struct TargetInfo {
        TargetFactory factory;
        std::string description;
    };
    std::unordered_map<std::string, TargetInfo> factories_;
};

template <typename T>
requires std::is_base_of_v<Target, T>
struct RegisterTarget {
    RegisterTarget(std::string const& name, std::string const& description = "") {
        TargetRegistry::get().registerTarget(name, [](){
            return std::make_unique<T>();
        }, description);
    }
};
