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

struct libusb_device *dfu_find_device( const uint32_t vendor,
                                       const uint32_t product,
                                       const uint32_t bus_number,
                                       const uint32_t device_address)
{
    libusb_device **list;
    size_t devicecount;
    extern libusb_context *usbcontext;
    libusb_device *device = NULL;

    TRACE( "%s( %u, %u )\n", __FUNCTION__, vendor, product);
    DEBUG( "%s(%08x, %08x)\n", __FUNCTION__, vendor, product );

    devicecount = libusb_get_device_list( usbcontext, &list );

    for( size_t i = 0; i < devicecount; i++ ) {
        libusb_device *dev = list[i];
        struct libusb_device_descriptor descriptor;

        if( libusb_get_device_descriptor(dev, &descriptor) ) {
             DEBUG( "Failed in libusb_get_device_descriptor\n" );
             break;
        }

        DEBUG( "%2d: 0x%04x, 0x%04x\n", (int) i,
                descriptor.idVendor, descriptor.idProduct );

        if( (vendor  == descriptor.idVendor) &&
            (product == descriptor.idProduct) &&
            ((bus_number == 0)
             || ((libusb_get_bus_number(dev) == bus_number) &&
                 (libusb_get_device_address(dev) == device_address))) )
        {
            device = dev;
            break;
        }
    }

    libusb_free_device_list( list, 1 );
    return device;
}

dfu_bool dfu_find_interface(struct libusb_device *device,
                            const dfu_bool honor_interfaceclass,
                            const uint8_t expected_protocol,
                            uint8_t *bConfigurationValue,
                            uint8_t *bInterfaceNumber)
{
    TRACE( "%s()\n", __FUNCTION__ );

    struct libusb_device_descriptor descriptor;

    if( libusb_get_device_descriptor(device, &descriptor) ) {
         DEBUG( "Failed in libusb_get_device_descriptor\n" );
         return false;
    }

    /* Loop through all of the configurations */
    for( int32_t c = 0; c < descriptor.bNumConfigurations; ++c ) {
        struct libusb_config_descriptor *config;

        if( libusb_get_config_descriptor(device, c, &config) ) {
            DEBUG( "can't get_config_descriptor: %d\n", c );
            return false;
        }
        DEBUG( "config %d: maxpower=%d*2 mA\n", c, config->MaxPower );

        /* Loop through all of the interfaces */
        for( int32_t i = 0; i < config->bNumInterfaces; i++ ) {
            struct libusb_interface interface;

            interface = config->interface[i];
            DEBUG( "interface %d\n", i );

            /* Loop through all of the settings */
            for( int32_t s = 0; s < interface.num_altsetting; s++ ) {
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
                            *bConfigurationValue = config->bConfigurationValue;
                            *bInterfaceNumber = setting.bInterfaceNumber;
                            return true;
                        } else {
                            // DEBUG( "Found DFU Inteface: %d, but protocol is incorrect: %d\n", setting.bInterfaceNumber, expected_protocol);
                            fprintf( stderr,  "Found DFU Inteface: %d, but protocol is incorrect: %d, expected: %d\n",
                                     setting.bInterfaceNumber, setting.bInterfaceProtocol, expected_protocol);
                        }
                    }
                } else {
                    /* If there is a bug in the DFU firmware, return the first
                     * found interface. */
                    DEBUG( "WARNING: Found DFU Interface: %d, but honor_interfaceclass is DISABLED\n", setting.bInterfaceNumber );
                    *bConfigurationValue = config->bConfigurationValue;
                    *bInterfaceNumber = setting.bInterfaceNumber;
                    return true;
                }
            }
        }

        libusb_free_config_descriptor( config );
    }

    return false;
}

void dfu_detach_drivers(libusb_device *device, dfu_device_t *dfu_device)
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

struct libusb_device *dfu_device_init( const uint32_t vendor,
                                       const uint32_t product,
                                       const uint32_t bus_number,
                                       const uint32_t device_address,
                                       dfu_device_t *dfu_device,
                                       const dfu_bool initial_abort,
                                       const dfu_bool honor_interfaceclass,
                                       const uint8_t expected_protocol )
{
    TRACE( "%s( %u, %u, %p, %s, %s, %d )\n", __FUNCTION__, vendor, product,
           dfu_device, (initial_abort ? "true" : "false"),
           (honor_interfaceclass ? "true" : "false"),
           expected_protocol);

    DEBUG( "%s(%08x, %08x)\n", __FUNCTION__, vendor, product );

    libusb_device * device = dfu_find_device(vendor, product, bus_number, device_address);
    if (device == NULL) {
        return NULL;
    }

    uint8_t bConfigurationValue = 0;
    uint8_t bInterfaceNumber = 0;

    if (!dfu_find_interface(device, honor_interfaceclass, expected_protocol, &bConfigurationValue, &bInterfaceNumber)) {
        return NULL;
    }

    dfu_device->interface = bInterfaceNumber;

    if (libusb_open(device, &dfu_device->handle)) {
        return NULL;
    }

    DEBUG( "opened interface %d...\n", dfu_device->interface );

    dfu_detach_drivers(device, dfu_device);

    if( 0 == libusb_set_configuration(dfu_device->handle, bConfigurationValue) ) {
        DEBUG( "set configuration %d...\n", bConfigurationValue);
        if( 0 == libusb_claim_interface(dfu_device->handle, dfu_device->interface) )
        {
            DEBUG( "claimed interface %d...\n", dfu_device->interface );

            if (expected_protocol == 1) {
                return device;
            }

            if ( 0 == dfu_make_idle(dfu_device, initial_abort) ) {
                return device;

            } else {
                DEBUG( "Failed to put the device in dfuIDLE mode.\n" );
                libusb_release_interface( dfu_device->handle, dfu_device->interface );
            }

        } else {
            DEBUG( "Failed to claim the DFU interface.\n" );
        }
    } else {
        DEBUG( "Failed to set configuration.\n" );
    }

    libusb_close(dfu_device->handle);
    dfu_device->handle = NULL;
    dfu_device->interface = 0;

    return NULL;
}
