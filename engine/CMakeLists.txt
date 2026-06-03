cmake_minimum_required(VERSION 3.24)

if(EMSCRIPTEN)
    project(SheetMetalNestingCpp VERSION 0.1.0 LANGUAGES CXX)
else()
    project(SheetMetalNestingCpp VERSION 0.1.0 LANGUAGES CXX RC)
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(WIN32)
    set(CMAKE_RC_COMPILE_OBJECT "<CMAKE_RC_COMPILER> /fo <OBJECT> <SOURCE>")
endif()

add_library(nesting_core
    src/core/dxf.cpp
    src/core/geometry.cpp
    src/core/nesting.cpp
    src/core/perf_log.cpp
    src/core/gpu_context.cpp
)
target_include_directories(nesting_core PUBLIC src)

add_executable(sheet_nest_cli src/cli/main.cpp)
target_link_libraries(sheet_nest_cli PRIVATE nesting_core)

if(WIN32)
    add_executable(sheet_nest_gui WIN32 src/gui/win32_app.cpp app.rc)
    target_link_libraries(sheet_nest_gui PRIVATE nesting_core user32 gdi32 gdiplus comdlg32 shell32 ole32 comctl32 wininet)
    target_link_options(sheet_nest_gui PRIVATE "-mwindows" "-static")
endif()

enable_testing()
add_executable(core_tests tests/core_tests.cpp)
target_link_libraries(core_tests PRIVATE nesting_core)
add_test(NAME core_tests COMMAND core_tests)

add_executable(test_shuffle test_shuffle.cpp)
target_link_libraries(test_shuffle PRIVATE nesting_core)

if(EMSCRIPTEN)
    add_executable(nesting_wasm src/wasm/wasm_binding.cpp)
    target_link_libraries(nesting_wasm PRIVATE nesting_core)
    
    # Emscripten specific linker flags
    set_target_properties(nesting_wasm PROPERTIES
        LINK_FLAGS "-s WASM=1 -s MODULARIZE=1 -s EXPORT_NAME=createNestingModule -s ALLOW_MEMORY_GROWTH=1 -s FORCE_FILESYSTEM=1 -s \"EXPORTED_RUNTIME_METHODS=['FS']\" --bind -O3"
        OUTPUT_NAME "nesting"
    )
endif()
