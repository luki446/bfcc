# AGENTS.md

## Cursor Cloud specific instructions

### Overview

**bfcc** is a C++23 CMake-based Brainfuck compiler/interpreter. See `README.md` for build/run commands.

### Build requirements

- **GCC 14+** is required for C++23 `<print>` support. GCC 13 does not include this header. Install with `sudo apt-get install -y g++-14`.
- **Boost git submodule** must be initialized: `git submodule update --init --recursive`. The `ext/boost/` directory will be empty otherwise.
- CMake 3.25+ (pre-installed on Ubuntu 24.04).

### Build commands

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_CXX_COMPILER=g++-14
cmake --build build -j$(nproc)
```

The binary is at `build/bfcc`.

### Running

- **Interpreter mode:** `./build/bfcc run examples/hello_world.bf`
- **IR dump:** `./build/bfcc build examples/hello_world.bf --emit-ir -O -v`
- The `bfcc build` subcommand targets Windows x64 only (produces x86_64 assembly for clang on Windows). On Linux, only interpreter mode (`run`) and IR dump (`--emit-ir`) work fully.
- The `--list-targets` flag reads stdin before printing targets. Pipe empty input: `echo "" | ./build/bfcc build --list-targets`

### Known caveats

- `runner.cc` needed `#include <cstdint>` added for GCC 14 compatibility (GCC 13/MSVC include it transitively).
- No automated test framework exists. Testing is manual: run the interpreter on `.bf` files in `examples/`.
- No linter is configured for this project.
