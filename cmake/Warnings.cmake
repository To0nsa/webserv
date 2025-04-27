# cmake/Warnings.cmake

# Function to enable common warning flags for a given target
function(enable_warnings target)
    target_compile_options(${target} PRIVATE
        -Wall
        -Wextra
        -Werror
        -Wshadow
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Wcast-align
        -Wunused
        -Woverloaded-virtual
        -Wpedantic
        -Wconversion
        -Wsign-conversion
        -Wnull-dereference
        -Wdouble-promotion
        -Wformat=2
    )
endfunction()