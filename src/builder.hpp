#pragma once

#include "ir.hpp"
#include <string>

IRProgram BuildIR(std::string const& source);

IRProgram OptimizeIR(IRProgram const& program);