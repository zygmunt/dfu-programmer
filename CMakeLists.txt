cmake_minimum_required(VERSION 2.6.0)

project(dfu-programmer LANGUAGES C)
set(VERSION 0.7.2.zx)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUSB REQUIRED libusb-1.0)

find_package(Git)

set(sources
    src/arguments.c
    src/atmel.c
    src/commands.c
    src/dfu.c
    src/intel_hex.c
    src/main.c
    src/stm32.c
    src/util.c
    src/usb.c
)

set(headers
    src/arguments.h
    src/atmel.h
    src/commands.h
    src/dfu-bool.h
    src/dfu-device.h
    src/dfu.h
    src/intel_hex.h
    src/stm32.h
    src/util.h
    src/usb.h
    src/version.h
)

if (GIT_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
    execute_process(COMMAND ${GIT_EXECUTABLE} -C ${CMAKE_SOURCE_DIR} rev-parse --short HEAD OUTPUT_VARIABLE GIT_HASH OUTPUT_STRIP_TRAILING_WHITESPACE)
else()
    set (GIT_HASH)
endif()

configure_file("${CMAKE_SOURCE_DIR}/src/version.c.in" "${CMAKE_BINARY_DIR}/version.c" @ONLY)

include_directories(${CMAKE_SOURCE_DIR}/src)
list(APPEND sources ${CMAKE_BINARY_DIR}/version.c)

add_executable(
    dfu-programmer
    ${sources}
)

target_link_libraries(dfu-programmer ${LIBUSB_LIBRARIES})
target_include_directories(dfu-programmer PUBLIC ${LIBUSB_INCLUDE_DIRS})
target_compile_options(dfu-programmer PUBLIC ${LIBUSB_CFLAGS_OTHER})
