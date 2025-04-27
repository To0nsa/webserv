# cmake/ClangTools.cmake

# Clang-Tidy integration (optional)
find_program(CLANG_TIDY_EXE NAMES "clang-tidy")
if(CLANG_TIDY_EXE)
    set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE};-checks=*,-clang-analyzer-alpha.*")
    message(STATUS "clang-tidy found: ${CLANG_TIDY_EXE}")
else()
    message(STATUS "clang-tidy not found, skipping static analysis setup.")
endif()

# Clang-specific compiler improvements
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(STATUS "Applying Clang-specific compile options.")

    # Helper macro to apply options to multiple targets safely
    macro(add_clang_compile_options target)
        if(TARGET ${target})
            target_compile_options(${target} PRIVATE -fcolor-diagnostics)
        endif()
    endmacro()

    # Apply to main executable and tests
    add_clang_compile_options(webserv)
    add_clang_compile_options(webserv_tests)
endif()
