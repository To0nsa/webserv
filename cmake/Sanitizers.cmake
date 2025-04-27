# cmake/Sanitizers.cmake

option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)

function(enable_sanitizers target)
    # Conflict detection
    if(ENABLE_ASAN AND ENABLE_TSAN)
        message(FATAL_ERROR "Cannot enable both AddressSanitizer (ASAN) and ThreadSanitizer (TSAN) at the same time.")
    endif()

    # Apply sanitizers
    if(ENABLE_ASAN)
        message(STATUS "AddressSanitizer enabled for target: ${target}")
        target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    endif()

    if(ENABLE_TSAN)
        message(STATUS "ThreadSanitizer enabled for target: ${target}")
        target_compile_options(${target} PRIVATE -fsanitize=thread -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE -fsanitize=thread -fno-omit-frame-pointer)
    endif()

    if(ENABLE_UBSAN)
        message(STATUS "UndefinedBehaviorSanitizer enabled for target: ${target}")
        target_compile_options(${target} PRIVATE -fsanitize=undefined -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE -fsanitize=undefined -fno-omit-frame-pointer)
    endif()

    # Warn if no sanitizer is enabled during Debug builds
    if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT (ENABLE_ASAN OR ENABLE_TSAN OR ENABLE_UBSAN))
        message(WARNING "No sanitizer enabled in Debug mode. Consider enabling ASAN, TSAN, or UBSAN for better debugging.")
    endif()
endfunction()



