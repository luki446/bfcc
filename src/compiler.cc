#include "compiler.hpp"

TargetRegistry& TargetRegistry::get() {
    static TargetRegistry instance{};
    return instance;
}

void TargetRegistry::registerTarget(std::string const& name, TargetFactory factory, std::string description) {
    factories_[name] = {std::move(factory), std::move(description)};
}

std::vector<std::string> TargetRegistry::getTargetsList() const {
    std::vector<std::string> targets{};
    targets.reserve(factories_.size());

    for (auto const& [name, info] : factories_) {
        targets.push_back(name);
    }

    return targets;
}

std::string TargetRegistry::getTargetDescription(std::string const& name) const {
    if (factories_.contains(name)) {
        return factories_.at(name).description;
    }
    return "";
}

std::unique_ptr<Target> TargetRegistry::createTarget(std::string const& name){
    if(factories_.contains(name)) {
        return factories_.at(name).factory();
    }

    return nullptr;
}
