TEMPLATE = aux 

HEADERS += \
    src/arguments.h \
    src/atmel.h \
    src/commands.h \
    src/dfu-bool.h \
    src/dfu-device.h \
    src/dfu.h \
    src/intel_hex.h \
    src/stm32.h \
    src/util.h \
    src/usb.h

SOURCES += \
    src/arguments.c \
    src/atmel.c \
    src/commands.c \
    src/dfu.c \
    src/intel_hex.c \
    src/main.c \
    src/stm32.c \
    src/util.c \
    src/usb.c

DEFINES += HAVE_LIBUSB_1_0
CONFIG += link_pkgconfig
PKGCONFIG += libusb
