cmake_minimum_required(VERSION 3.10)
project(pm LANGUAGES C)

if(NOT DEFINED SROBOTIS_EXPORT_SET)
    set(SROBOTIS_EXPORT_SET "ComponentsTargets" CACHE STRING "Export set name used by exported targets")
endif()

# Set C standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/drivers
)

# Core source files
set(PM_CORE_SOURCES
    src/pm_core.c
)

# Driver source files
set(PM_DRIVER_SOURCES "")
list(FIND SROBOTIS_PERIPHERALS_PM_ENABLED_DRIVERS "drv_generic" _pm_generic_idx)
if(_pm_generic_idx EQUAL -1)
    list(APPEND SROBOTIS_PERIPHERALS_PM_ENABLED_DRIVERS drv_generic)
endif()
foreach(_drv IN LISTS SROBOTIS_PERIPHERALS_PM_ENABLED_DRIVERS)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/drivers/${_drv}.c")
        list(APPEND PM_DRIVER_SOURCES "src/drivers/${_drv}.c")
    else()
        message(FATAL_ERROR "pm: enabled driver '${_drv}' not found at src/drivers/${_drv}.c")
    endif()
endforeach()

# All source files
set(PM_SOURCES
    ${PM_CORE_SOURCES}
    ${PM_DRIVER_SOURCES}
)

# Create shared library (drivers auto-register via constructor)
add_library(pm SHARED ${PM_SOURCES})
find_package(Threads REQUIRED)
target_link_libraries(pm PRIVATE Threads::Threads)

# Set compiler flags
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(pm PRIVATE
        -Wall
        -Wextra
    )
endif()

# Install rules
install(TARGETS pm
    EXPORT ${SROBOTIS_EXPORT_SET}
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(FILES include/pm.h
    DESTINATION include
)

# Optional: Create test executables
option(BUILD_TESTS "Build test programs" OFF)

if(BUILD_TESTS)
    add_executable(test_pm_adc test/test_pm_adc.c)
    target_link_libraries(test_pm_adc pm)
    set_target_properties(test_pm_adc PROPERTIES BUILD_RPATH "$ORIGIN/../lib")
    install(TARGETS test_pm_adc RUNTIME DESTINATION bin)

    add_executable(test_pm_generic test/test_pm_generic.c)
    target_link_libraries(test_pm_generic pm)
    set_target_properties(test_pm_generic PROPERTIES BUILD_RPATH "$ORIGIN/../lib")
    install(TARGETS test_pm_generic RUNTIME DESTINATION bin)
endif()
