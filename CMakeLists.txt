# CMakeLists.txt - Improved version based on your setup

cmake_minimum_required(VERSION 3.16)

# Project definition
project(webserv
    VERSION 1.0
    DESCRIPTION "HTTP Web Server project (Hive School)"
    LANGUAGES CXX
)

# Generate compile_commands.json for tooling (clang-tidy, IDEs, etc.)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Load custom CMake modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(CompilerOptions)
include(Warnings)
include(Sanitizers)
include(ClangTools)
include(CTestSettings)

# Enable CTest (must come after project() and before add_test)
enable_testing()

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Output directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Handle multi-config generators (Visual Studio, Xcode, etc.)
if(CMAKE_CONFIGURATION_TYPES)
    foreach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
        string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG)
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/bin/${OUTPUTCONFIG})
    endforeach()
endif()

# Include project headers
include_directories(include)

# Find all source files
file(GLOB_RECURSE SOURCES
    src/*.cpp
)

# Main executable
add_executable(webserv ${SOURCES})
target_include_directories(webserv PRIVATE ${PROJECT_SOURCE_DIR}/include)

# Apply options to main target
enable_warnings(webserv)
enable_sanitizers(webserv)

# Find all test source files
file(GLOB TEST_SOURCES
    tests/*.cpp
)

# Test executable
add_executable(webserv_tests ${TEST_SOURCES})
target_include_directories(webserv_tests PRIVATE ${PROJECT_SOURCE_DIR}/include)

# Apply options to test target
enable_warnings(webserv_tests)
enable_sanitizers(webserv_tests)

# Register tests
register_test(webserv_tests webserv_tests)

# Info summary
message(STATUS "Project Name: ${PROJECT_NAME}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Output Directory: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
