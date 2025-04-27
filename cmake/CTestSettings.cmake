# cmake/CTestSettings.cmake

# Minimum required for CTest
include(CTest)

# Global test properties
set(CTEST_OUTPUT_ON_FAILURE ON) # Always print test output on failure

# Default timeout for each test (in seconds)
set_property(GLOBAL PROPERTY TIMEOUT 10)

# Function to register a test with reasonable defaults
function(register_test name executable)
    add_test(NAME ${name} COMMAND ${executable})

    set(options)
    set(oneValueArgs LABEL)
    cmake_parse_arguments(TEST "${options}" "${oneValueArgs}" "" ${ARGN})

    set_tests_properties(${name} PROPERTIES
        TIMEOUT 10
    )

    if(TEST_LABEL)
        set_tests_properties(${name} PROPERTIES LABELS "${TEST_LABEL}")
    endif()
endfunction()
