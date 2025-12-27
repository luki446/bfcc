#include "compiler.hpp"

TargetRegistry& TargetRegistry::get() {
    static TargetRegistry instance{};
    return instance;
}

void TargetRegistry::registerTarget(std::string const& name, TargetFactory factory) {
    factories_[name] = std::move(factory);
}

std::vector<std::string> TargetRegistry::getTargetsList() {
    std::vector<std::string> targets{};
    targets.reserve(factories_.size());

    for (auto const& [name, factory] : factories_) {
        targets.push_back(name);
    }
    
    return targets;
}

std::unique_ptr<Target> TargetRegistry::createTarget(std::string const& name){
    if(factories_.contains(name)) {
        return factories_[name]();
    }

    return nullptr;
}