#include "usb.h"

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <libusb.h>
#include <errno.h>
#include "dfu.h"
#include "util.h"
#include "dfu-bool.h"

#define DFU_DEBUG_THRESHOLD         100
#define DFU_TRACE_THRESHOLD         200
#define DFU_MESSAGE_DEBUG_THRESHOLD 300

#define DEBUG(...)  dfu_debug( __FILE__, __FUNCTION__, __LINE__, \
                               DFU_DEBUG_THRESHOLD, __VA_ARGS__ )
#define TRACE(...)  dfu_debug( __FILE__, __FUNCTION__, __LINE__, \
                               DFU_TRACE_THRESHOLD, __VA_ARGS__ )
#define MSG_DEBUG(...)  dfu_debug( __FILE__, __FUNCTION__, __LINE__, \
                               DFU_MESSAGE_DEBUG_THRESHOLD, __VA_ARGS__ )

int32_t dfu_find_interface( struct libusb_device *device,
                            const dfu_bool honor_interfaceclass,
                            const uint8_t bNumConfigurations,
                            const uint8_t expected_protocol ) {
    int32_t c,i,s;

    TRACE( "%s()\n", __FUNCTION__ );

    /* Loop through all of the configurations */
    for( c = 0; c < bNumConfigurations; c++ ) {
        struct libusb_config_descriptor *config;

        if( libusb_get_config_descriptor(device, c, &config) ) {
            DEBUG( "can't get_config_descriptor: %d\n", c );
            return -1;
        }
        DEBUG( "config %d: maxpower=%d*2 mA\n", c, config->MaxPower );

        /* Loop through all of the interfaces */
        for( i = 0; i < config->bNumInterfaces; i++ ) {
            struct libusb_interface interface;

            interface = config->interface[i];
            DEBUG( "interface %d\n", i );

            /* Loop through all of the settings */
            for( s = 0; s < interface.num_altsetting; s++ ) {
                struct libusb_interface_descriptor setting;

                setting = interface.altsetting[s];
                DEBUG( "setting %d: class:%d, subclass %d, protocol:%d\n", s,
                                setting.bInterfaceClass, setting.bInterfaceSubClass,
                                setting.bInterfaceProtocol );

                if( honor_interfaceclass ) {
                    /* Check if the interface is a DFU interface */
                    if(    (USB_CLASS_APP_SPECIFIC == setting.bInterfaceClass)
                        && (DFU_SUBCLASS == setting.bInterfaceSubClass) )
                    {
                        if (expected_protocol == setting.bInterfaceProtocol) {
                            DEBUG( "Found DFU Inteface: %d\n", setting.bInterfaceNumber );
                            return setting.bInterfaceNumber;
                        } else {
                            // DEBUG( "Found DFU Inteface: %d, but protocol is incorrect: %d\n", setting.bInterfaceNumber, expected_protocol);
                            fprintf( stderr,  "Found DFU Inteface: %d, but protocol is incorrect: %d, expected: %d\n",
                                     setting.bInterfaceNumber, setting.bInterfaceProtocol, expected_protocol);
                        }
                    }
                } else {
                    /* If there is a bug in the DFU firmware, return the first
                     * found interface. */
                    DEBUG( "Found DFU Interface: %d\n", setting.bInterfaceNumber );
                    return setting.bInterfaceNumber;
                }
            }
        }

        libusb_free_config_descriptor( config );
    }

    return -1;
}

struct libusb_device *dfu_device_init( const uint32_t vendor,
                                       const uint32_t product,
                                       const uint32_t bus_number,
                                       const uint32_t device_address,
                                       dfu_device_t *dfu_device,
                                       const dfu_bool initial_abort,
                                       const dfu_bool honor_interfaceclass,
                                       const uint8_t expected_protocol ) {
    libusb_device **list;
    size_t i,devicecount;
    extern libusb_context *usbcontext;
    int32_t retries = 4;

    TRACE( "%s( %u, %u, %p, %s, %s, %d )\n", __FUNCTION__, vendor, product,
           dfu_device, ((true == initial_abort) ? "true" : "false"),
           (honor_interfaceclass ? "true" : "false"),
           expected_protocol);

    DEBUG( "%s(%08x, %08x)\n",__FUNCTION__, vendor, product );

retry:
    devicecount = libusb_get_device_list( usbcontext, &list );

    for( i = 0; i < devicecount; i++ ) {
        libusb_device *device = list[i];
        struct libusb_device_descriptor descriptor;

        if( libusb_get_device_descriptor(device, &descriptor) ) {
             DEBUG( "Failed in libusb_get_device_descriptor\n" );
             break;
        }

        DEBUG( "%2d: 0x%04x, 0x%04x\n", (int) i,
                descriptor.idVendor, descriptor.idProduct );

        if( (vendor  == descriptor.idVendor) &&
            (product == descriptor.idProduct) &&
            ((bus_number == 0)
             || ((libusb_get_bus_number(device) == bus_number) &&
                 (libusb_get_device_address(device) == device_address))) )
        {
            int32_t tmp;
            DEBUG( "found device at USB:%d,%d\n", libusb_get_bus_number(device), libusb_get_device_address(device) );
            /* We found a device that looks like it matches...
             * let's try to find the DFU interface, open the device
             * and claim it. */
            tmp = dfu_find_interface( device, honor_interfaceclass,
                                      descriptor.bNumConfigurations,
                                      expected_protocol );

            if( 0 <= tmp ) {    /* The interface is valid. */
                dfu_device->interface = tmp;

                if( 0 == libusb_open(device, &dfu_device->handle) ) {
                    DEBUG( "opened interface %d...\n", tmp );

                    // HERE
                    {
                        struct libusb_config_descriptor *config = NULL;
                        if (libusb_get_active_config_descriptor(device, &config) == 0)
                        {
                            for (int i = 0; i < config->bNumInterfaces; ++i) {
                                if (libusb_kernel_driver_active(dfu_device->handle, i) == 1)
                                {
                                    libusb_detach_kernel_driver(dfu_device->handle, i);
                                }
                            }
                            libusb_free_config_descriptor(config);
                        }
                    }

                    // FIXME: hardcoded configuration number !!!
                    if( 0 == libusb_set_configuration(dfu_device->handle, 1) ) {
                        DEBUG( "set configuration %d...\n", 1 );
                        if( 0 == libusb_claim_interface(dfu_device->handle, dfu_device->interface) )
                        {
                            DEBUG( "claimed interface %d...\n", dfu_device->interface );

                            if (expected_protocol == 1) {
                                return device;
                            }

                            switch( dfu_make_idle(dfu_device, initial_abort) )
                            {
                                case 0:
                                    libusb_free_device_list( list, 1 );
                                    return device;

                                case 1:
                                    retries--;
                                    libusb_free_device_list( list, 1 );
                                    goto retry;
                            }

                            DEBUG( "Failed to put the device in dfuIDLE mode.\n" );
                            libusb_release_interface( dfu_device->handle, dfu_device->interface );
                            retries = 4;
                        } else {
                            DEBUG( "Failed to claim the DFU interface.\n" );
                        }
                    } else {
                        DEBUG( "Failed to set configuration.\n" );
                    }

                    libusb_close(dfu_device->handle);
                }
            }
        }
    }

    libusb_free_device_list( list, 1 );
    dfu_device->handle = NULL;
    dfu_device->interface = 0;

    return NULL;
}
