#ifndef __USB_H__
#define __USB_H__

#include <libusb.h>

#include "dfu.h"

// ________  P R O T O T Y P E S  _______________________________
int32_t dfu_find_interface( struct libusb_device *device,
                            const dfu_bool honor_interfaceclass,
                            const uint8_t bNumConfigurations,
                            const uint8_t expected_protocol);
/*  Used to find the dfu interface for a device if there is one.
 *
 *  device - the device to search
 *  honor_interfaceclass - if the actual interface class information
 *                         should be checked, or ignored (bug in device DFU code)
 *
 *  returns the interface number if found, < 0 otherwise
 */

libusb_device *dfu_device_init( const uint32_t vendor,
                                const uint32_t product,
                                const uint32_t bus,
                                const uint32_t dev_addr,
                                dfu_device_t *device,
                                const dfu_bool initial_abort,
                                const dfu_bool honor_interfaceclass,
                                const uint8_t expected_protocol);
/*  dfu_device_init is designed to find one of the usb devices which match
 *  the vendor and product parameters passed in.
 *
 *  vendor  - the vender number of the device to look for
 *  product - the product number of the device to look for
 *  [out] device - the dfu device to commmunicate with
 *
 *  return a pointer to the usb_device if found, or NULL otherwise
 */

#endif // __USB_H__
