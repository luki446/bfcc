#pragma once

#include <filesystem>
#include <functional>
#include <type_traits>
#include <unordered_map>

#include "ir.hpp"

class Target {
public:
    virtual void compile(IRProgram const& program, std::filesystem::path const& outputPath) = 0;
    virtual ~Target() = default;
};

using TargetFactory = std::function<std::unique_ptr<Target>()>;

class TargetRegistry {
public:
    static TargetRegistry& get();
    void registerTarget(std::string const& name, TargetFactory factory);
    std::vector<std::string> getTargetsList();
    std::unique_ptr<Target> createTarget(std::string const& name);
private:
    std::unordered_map<std::string, TargetFactory> factories_;
};

template <typename T>
requires std::is_base_of_v<Target, T>
struct RegisterTarget {
    RegisterTarget(std::string const& name) {
        TargetRegistry::get().registerTarget(name, [](){
            return std::make_unique<T>();
        });
    }
};