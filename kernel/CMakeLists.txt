add_library(pico_kernel STATIC
    kernel.c
    kimpl.c
    context.s
    klogging.c
    schedule.c
)

# Expose only kernel.h to consumers.
target_include_directories(pico_kernel
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/internal
)

# Link Pico SDK libs here so the root program only needs to link pico_kernel.
target_link_libraries(pico_kernel
    PUBLIC
        pico_stdlib
        hardware_timer
)

if(NOT DEFINED LOG_LEVEL)
    message(WARNING "LOG_LEVEL is not defined. Please add 'set(LOG_LEVEL <level>)' to your CMakeLists.txt")
endif()
