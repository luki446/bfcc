#pragma once

#include <cstdint>
#include <string>
#include <variant>
#include <vector>
#include <format>

struct SourceLocation {
    int32_t line = 1;
    int32_t column = 1;

    std::string toString() const {
        return std::format("{}:{}", line, column);
    }
};

class MovePtr {
public:
    constexpr MovePtr(int32_t offset) : offset_(offset) {}
    constexpr MovePtr(int32_t offset, SourceLocation loc) : offset_(offset), location_(loc) {}
    constexpr int32_t offset() const { return offset_; }
    constexpr SourceLocation location() const { return location_; }
private:
    int32_t offset_;
    SourceLocation location_;
};

class ChangeValue {
public:
    constexpr ChangeValue(int32_t delta) : delta_(delta) {}
    constexpr ChangeValue(int32_t delta, SourceLocation loc) : delta_(delta), location_(loc) {}
    constexpr int32_t delta() const { return delta_; }
    constexpr SourceLocation location() const { return location_; }
private:
    int32_t delta_;
    SourceLocation location_;
};


class OutputCharacter {
public:
    constexpr OutputCharacter() = default;
    constexpr OutputCharacter(SourceLocation loc) : location_(loc) {}
    constexpr SourceLocation location() const { return location_; }
private:
    SourceLocation location_;
};

class InputCharacter {
public:
    constexpr InputCharacter() = default;
    constexpr InputCharacter(SourceLocation loc) : location_(loc) {}
    constexpr SourceLocation location() const { return location_; }
private:
    SourceLocation location_;
};

class LoopStart {
public:
    constexpr LoopStart(int32_t jump_offset) : jump_offset_(jump_offset) {}
    constexpr LoopStart(int32_t jump_offset, SourceLocation loc) : jump_offset_(jump_offset), location_(loc) {}
    constexpr int32_t jump_offset() const { return jump_offset_; }
    constexpr SourceLocation location() const { return location_; }
private:
    int32_t jump_offset_; // Positive offset to matching LoopEnd
    SourceLocation location_;
};

class LoopEnd {
public:
    constexpr LoopEnd(int32_t jump_offset) : jump_offset_(jump_offset) {}
    constexpr LoopEnd(int32_t jump_offset, SourceLocation loc) : jump_offset_(jump_offset), location_(loc) {}
    constexpr int32_t jump_offset() const { return jump_offset_; }
    constexpr SourceLocation location() const { return location_; }
private:
    int32_t jump_offset_; // Negative offset to matching LoopStart
    SourceLocation location_;
};


using IRInstruction = std::variant<
    MovePtr,
    ChangeValue,
    OutputCharacter,
    InputCharacter,
    LoopStart,
    LoopEnd
>;

using IRProgram = std::vector<IRInstruction>;


template<>
struct std::formatter<MovePtr> : std::formatter<std::string> {
    constexpr auto format(const MovePtr& mp, auto& ctx) const {
        return std::formatter<std::string>::format("MovePtr(offset=" + std::to_string(mp.offset()) + ") @" + mp.location().toString(), ctx);
    }
};

template<>
struct std::formatter<ChangeValue> : std::formatter<std::string> {
    constexpr auto format(const ChangeValue& cv, auto& ctx) const{
        return std::formatter<std::string>::format("ChangeValue(delta=" + std::to_string(cv.delta()) + ") @" + cv.location().toString(), ctx);
    }
};

template<>
struct std::formatter<OutputCharacter> : std::formatter<std::string> {
    constexpr auto format(const OutputCharacter& oc, auto& ctx) const {
        return std::formatter<std::string>::format("OutputCharacter @" + oc.location().toString(), ctx);
    }
};

template<>
struct std::formatter<InputCharacter> : std::formatter<std::string> {
    constexpr auto format(const InputCharacter& ic, auto& ctx) const {
        return std::formatter<std::string>::format("InputCharacter @" + ic.location().toString(), ctx);
    }
};

template<>
struct std::formatter<LoopStart> : std::formatter<std::string> {
    constexpr auto format(const LoopStart& ls, auto& ctx) const {
        return std::formatter<std::string>::format("LoopStart(jump_offset=" + std::to_string(ls.jump_offset()) + ") @" + ls.location().toString(), ctx);
    }
};

template<>
struct std::formatter<LoopEnd> : std::formatter<std::string> {
    constexpr auto format(const LoopEnd& le, auto& ctx) const {
        return std::formatter<std::string>::format("LoopEnd(jump_offset=" + std::to_string(le.jump_offset()) + ") @" + le.location().toString(), ctx);
    }
};

template<>
struct std::formatter<IRInstruction> : std::formatter<std::string> {
    constexpr auto format(const IRInstruction& ir, auto& ctx) const {
        return std::visit([&ctx](const auto& instr) {
            return std::format_to(ctx.out(), "{}", instr);
        }, ir);
    }
};

template<>
struct std::formatter<IRProgram> : std::formatter<std::string> {
    constexpr auto format(const IRProgram& program, auto& ctx) const {
        for(size_t i = 0; i < program.size(); ++i) {
            std::format_to(ctx.out(), "{:05}: {}\n", i, program[i]);
        }
        return ctx.out();
    }
};