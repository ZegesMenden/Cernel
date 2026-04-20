file(GLOB KERNEL_SRC ${CMAKE_CURRENT_LIST_DIR}/src/*.c)

# This should change in the future so we can define different implementations
file(GLOB KERNEL_IMPL ${CMAKE_CURRENT_LIST_DIR}/impl/*.c ${CMAKE_CURRENT_LIST_DIR}/impl/*.s)

add_subdirectory(components/dload)

add_library(pico_kernel STATIC ${KERNEL_SRC} ${KERNEL_IMPL})

# Expose only kernel.h to consumers.
target_include_directories(pico_kernel
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/include
        ${CMAKE_CURRENT_LIST_DIR}/components
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/internal
)

# Link Pico SDK libs here so the root program only needs to link pico_kernel.
target_link_libraries(pico_kernel
    PUBLIC
        pico_stdlib
        hardware_timer
        hardware_riscv_platform_timer
        hardware_hazard3
    dload
)

if(NOT DEFINED LOG_LEVEL)
    message(ERROR "LOG_LEVEL is not defined. Please add 'set(LOG_LEVEL <level>)' to your CMakeLists.txt")
endif()
