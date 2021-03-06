/*
 * dfu-programmer
 *
 * $Id$
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <stdio.h>
#include <string.h>
#include <libusb.h>

#include "dfu-device.h"
#include "dfu.h"
#include "atmel.h"
#include "arguments.h"
#include "commands.h"
#include "usb.h"
#include "version.h"


int debug;
libusb_context *usbcontext;

int main( int argc, char **argv )
{
    int retval = SUCCESS;
    int status;
    dfu_device_t dfu_device;
    struct programmer_arguments args;
    struct libusb_device *device = NULL;

    memset( &args, 0, sizeof(args) );
    memset( &dfu_device, 0, sizeof(dfu_device) );

    status = parse_arguments(&args, argc, argv);
    if( status < 0 ) {
        /* Exit with an error. */
        return ARGUMENT_ERROR;
    } else if (status > 0) {
        /* It was handled by parse_arguments. */
        return SUCCESS;
    }

    if (libusb_init(&usbcontext)) {
        fprintf( stderr, "%s: can't init libusb.\n", dfu_programmer_name );
        return DEVICE_ACCESS_ERROR;
    }

    if( debug >= 200 ) {
        libusb_set_option(usbcontext, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL_DEBUG);
    }

    if( !(args.command == com_bin2hex || args.command == com_hex2bin) ) {
        device = dfu_device_init( args.vendor_id, args.chip_id,
                                  args.bus_id, args.device_address,
                                  &dfu_device,
                                  args.initial_abort,
                                  args.honor_interfaceclass,
                                  args.command == com_dfumode ? DFU_PROTOCOL_RUNTIME : DFU_PROTOCOL_DFUMODE );

        if( NULL == device ) {
            fprintf( stderr, "%s: no device present.\n", dfu_programmer_name );
            retval = DEVICE_ACCESS_ERROR;
            goto error;
        }
    }

    if( 0 != (retval = execute_command(&dfu_device, &args)) ) {
        /* command issued a specific diagnostic already */
        goto error;
    }

error:
    if( NULL != dfu_device.handle ) {
        int rv;

        rv = libusb_release_interface( dfu_device.handle, dfu_device.interface );

        /* The RESET command sometimes causes the usb_release_interface command to fail.
           It is not obvious why this happens but it may be a glitch due to the hardware
           reset in the attached device. In any event, since reset causes a USB detach
           this should not matter, so there is no point in raising an alarm.
        */
        if( 0 != rv && !(com_launch == args.command &&
                args.com_launch_config.noreset == 0) ) {
            fprintf( stderr, "%s: failed to release interface %d.\n",
                             dfu_programmer_name, dfu_device.interface );
            retval = DEVICE_ACCESS_ERROR;
        }
    }

    if( NULL != dfu_device.handle ) {
        libusb_close(dfu_device.handle);
    }

    libusb_exit(usbcontext);

    return retval;
}
