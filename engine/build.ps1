$ErrorActionPreference = "Stop"

$compiler = (Resolve-Path -LiteralPath "tools\zig-cxx.cmd").Path
$ar = (Resolve-Path -LiteralPath "tools\zig-ar.cmd").Path
$ranlib = (Resolve-Path -LiteralPath "tools\zig-ranlib.cmd").Path
$rc = (Resolve-Path -LiteralPath "tools\zig-rc.cmd").Path
$rc = $rc -replace "\\", "/"

cmake -S . -B build -G Ninja `
    -DCMAKE_BUILD_TYPE=Release `
    -DCMAKE_CXX_COMPILER="$compiler" `
    -DCMAKE_AR="$ar" `
    -DCMAKE_RANLIB="$ranlib" `
    -DCMAKE_RC_COMPILER="$rc"

cmake --build build
ctest --test-dir build --output-on-failure
