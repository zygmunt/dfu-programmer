cmake_minimum_required(VERSION 2.6.0)

project(dfu-programmer C)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUSB REQUIRED libusb-1.0)

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
)

add_executable(
    dfu-programmer
    ${sources}
) 

target_link_libraries(dfu-programmer ${LIBUSB_LIBRARIES})
target_include_directories(dfu-programmer PUBLIC ${LIBUSB_INCLUDE_DIRS})
target_compile_options(dfu-programmer PUBLIC ${LIBUSB_CFLAGS_OTHER})
