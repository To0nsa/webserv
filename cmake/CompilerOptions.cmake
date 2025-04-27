# cmake/CompilerOptions.cmake

# Set default build type if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING
        "Choose the type of build (Debug, Release, RelWithDebInfo, MinSizeRel)." FORCE)
endif()

# Compiler warnings are configured separately (see Warnings.cmake)
# Here we handle general compiler behaviors

# Detect the compiler and set options accordingly
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # General flags for GCC/Clang
    set(COMMON_FLAGS "-Wall -Wextra -Werror")

    # Debug-specific flags
    set(CMAKE_CXX_FLAGS_DEBUG "-g3 -O0 -DDEBUG")

    # Release-specific flags
    set(CMAKE_CXX_FLAGS_RELEASE "-O3")

    # Additional common flags can be set here if needed
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    # Microsoft Visual C++
    set(COMMON_FLAGS "/W4 /WX")

    # Debug-specific flags
    set(CMAKE_CXX_FLAGS_DEBUG "/Zi /Od /DDEBUG")

    # Release-specific flags
    set(CMAKE_CXX_FLAGS_RELEASE "/O2")
endif()

# Apply common flags to all targets
add_compile_options(${COMMON_FLAGS})
