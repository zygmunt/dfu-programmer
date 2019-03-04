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

static uint16_t transaction = 0;

// ________  F U N C T I O N S  _______________________________
void dfu_set_transaction_num( uint16_t newnum ) {
    TRACE( "%s( %u )\n", __FUNCTION__, newnum );
    transaction = newnum;
    DEBUG("wValue set to %d\n", transaction);
}

uint16_t dfu_get_transaction_num( void ) {
    TRACE( "%s( %u )\n", __FUNCTION__ );
    return transaction;
}

int32_t dfu_detach( dfu_device_t *device, const int32_t timeout ) {
    int32_t result;

    TRACE( "%s( %p, %d )\n", __FUNCTION__, device, timeout );

    if( (NULL == device) || (NULL == device->handle) || (timeout < 0) ) {
        DEBUG( "Invalid parameter\n" );
        return -1;
    }

    result = dfu_transfer_out( device, DFU_DETACH, timeout, NULL, 0 );

    dfu_msg_response_output( __FUNCTION__, result );

    return result;
}

int32_t dfu_download( dfu_device_t *device,
                      const size_t length,
                      uint8_t* data ) {
    int32_t result;

    TRACE( "%s( %p, %u, %p )\n", __FUNCTION__, device, length, data );

    /* Sanity checks */
    if( (NULL == device) || (NULL == device->handle) ) {
        DEBUG( "Invalid parameter\n" );
        return -1;
    }

    if( (0 != length) && (NULL == data) ) {
        DEBUG( "data was NULL, but length != 0\n" );
        return -2;
    }

    if( (0 == length) && (NULL != data) ) {
        DEBUG( "data was not NULL, but length == 0\n" );
        return -3;
    }

    {
        size_t i;
        for( i = 0; i < length; i++ ) {
            MSG_DEBUG( "Message: m[%u] = 0x%02x\n", i, data[i] );
        }
    }

    result = dfu_transfer_out( device, DFU_DNLOAD, transaction++, data, length );

    dfu_msg_response_output( __FUNCTION__, result );

    return result;
}

int32_t dfu_upload( dfu_device_t *device, const size_t length, uint8_t* data ) {
    int32_t result;

    TRACE( "%s( %p, %u, %p )\n", __FUNCTION__, device, length, data );

    /* Sanity checks */
    if( (NULL == device) || (NULL == device->handle) ) {
        DEBUG( "Invalid parameter\n" );
        return -1;
    }

    if( (0 == length) || (NULL == data) ) {
        DEBUG( "data was NULL, or length is 0\n" );
        return -2;
    }

    result = dfu_transfer_in( device, DFU_UPLOAD, transaction++, data, length );

    dfu_msg_response_output( __FUNCTION__, result );

    return result;
}

int32_t dfu_get_status( dfu_device_t *device, dfu_status_t *status ) {
    uint8_t buffer[6];
    int32_t result;

    TRACE( "%s( %p, %p )\n", __FUNCTION__, device, status );

    if( (NULL == device) || (NULL == device->handle) ) {
        DEBUG( "Invalid parameter\n" );
        return -1;
    }

    /* Initialize the status data structure */
    status->bStatus       = DFU_STATUS_ERROR_UNKNOWN;
    status->bwPollTimeout = 0;
    status->bState        = STATE_DFU_ERROR;
    status->iString       = 0;

    result = dfu_transfer_in( device, DFU_GETSTATUS, 0, buffer, sizeof(buffer) );

    dfu_msg_response_output( __FUNCTION__, result );

    if( 6 == result ) {
        status->bStatus = buffer[0];
        status->bwPollTimeout = ((0xff & buffer[3]) << 16) |
                                ((0xff & buffer[2]) << 8)  |
                                (0xff & buffer[1]);

        status->bState  = buffer[4];
        status->iString = buffer[5];

        DEBUG( "==============================\n" );
        DEBUG( "status->bStatus: %s (0x%02x)\n",
               dfu_status_to_string(status->bStatus), status->bStatus );
        DEBUG( "status->bwPollTimeout: 0x%04x ms\n", status->bwPollTimeout );
        DEBUG( "status->bState: %s (0x%02x)\n",
               dfu_state_to_string(status->bState), status->bState );
        DEBUG( "status->iString: 0x%02x\n", status->iString );
        DEBUG( "------------------------------\n" );
    } else {
        if( 0 < result ) {
            /* There was an error, we didn't get the entire message. */
            DEBUG( "result: %d\n", result );
            return -2;
        }
    }

    return 0;
}

int32_t dfu_clear_status( dfu_device_t *device ) {
    int32_t result;

    TRACE( "%s( %p )\n", __FUNCTION__, device );

    if( (NULL == device) || (NULL == device->handle) ) {
        DEBUG( "Invalid parameter\n" );
        return -1;
    }

    result = dfu_transfer_out( device, DFU_CLRSTATUS, 0, NULL, 0 );

    dfu_msg_response_output( __FUNCTION__, result );

    return result;
}

int32_t dfu_get_state( dfu_device_t *device ) {
    int32_t result;
    uint8_t buffer[1];

    TRACE( "%s( %p )\n", __FUNCTION__, device );

    if( (NULL == device) || (NULL == device->handle) ) {
        DEBUG( "Invalid parameter\n" );
        return -1;
    }

    result = dfu_transfer_in( device, DFU_GETSTATE, 0, buffer, sizeof(buffer) );

    dfu_msg_response_output( __FUNCTION__, result );

    /* Return the error if there is one. */
    if( result < 1 ) {
        return result;
    }

    /* Return the state. */
    return buffer[0];
}

int32_t dfu_abort( dfu_device_t *device ) {
    int32_t result;

    TRACE( "%s( %p )\n", __FUNCTION__, device );

    if( (NULL == device) || (NULL == device->handle) ) {
        DEBUG( "Invalid parameter\n" );
        return -1;
    }

    result = dfu_transfer_out( device, DFU_ABORT, 0, NULL, 0 );

    dfu_msg_response_output( __FUNCTION__, result );

    return result;
}

char* dfu_state_to_string( const int32_t state ) {
    char *message = "unknown state";

    switch( state ) {
        case STATE_APP_IDLE:
            message = "appIDLE";
            break;
        case STATE_APP_DETACH:
            message = "appDETACH";
            break;
        case STATE_DFU_IDLE:
            message = "dfuIDLE";
            break;
        case STATE_DFU_DOWNLOAD_SYNC:
            message = "dfuDNLOAD-SYNC";
            break;
        case STATE_DFU_DOWNLOAD_BUSY:
            message = "dfuDNBUSY";
            break;
        case STATE_DFU_DOWNLOAD_IDLE:
            message = "dfuDNLOAD-IDLE";
            break;
        case STATE_DFU_MANIFEST_SYNC:
            message = "dfuMANIFEST-SYNC";
            break;
        case STATE_DFU_MANIFEST:
            message = "dfuMANIFEST";
            break;
        case STATE_DFU_MANIFEST_WAIT_RESET:
            message = "dfuMANIFEST-WAIT-RESET";
            break;
        case STATE_DFU_UPLOAD_IDLE:
            message = "dfuUPLOAD-IDLE";
            break;
        case STATE_DFU_ERROR:
            message = "dfuERROR";
            break;
    }

    return message;
}

char* dfu_status_to_string( const int32_t status ) {
    char *message = "unknown status";

    switch( status ) {
        case DFU_STATUS_OK:
            message = "OK";
            break;
        case DFU_STATUS_ERROR_TARGET:
            message = "errTARGET";
            break;
        case DFU_STATUS_ERROR_FILE:
            message = "errFILE";
            break;
        case DFU_STATUS_ERROR_WRITE:
            message = "errWRITE";
            break;
        case DFU_STATUS_ERROR_ERASE:
            message = "errERASE";
            break;
        case DFU_STATUS_ERROR_CHECK_ERASED:
            message = "errCHECK_ERASED";
            break;
        case DFU_STATUS_ERROR_PROG:
            message = "errPROG";
            break;
        case DFU_STATUS_ERROR_VERIFY:
            message = "errVERIFY";
            break;
        case DFU_STATUS_ERROR_ADDRESS:
            message = "errADDRESS";
            break;
        case DFU_STATUS_ERROR_NOTDONE:
            message = "errNOTDONE";
            break;
        case DFU_STATUS_ERROR_FIRMWARE:
            message = "errFIRMWARE";
            break;
        case DFU_STATUS_ERROR_VENDOR:
            message = "errVENDOR";
            break;
        case DFU_STATUS_ERROR_USBR:
            message = "errUSBR";
            break;
        case DFU_STATUS_ERROR_POR:
            message = "errPOR";
            break;
        case DFU_STATUS_ERROR_UNKNOWN:
            message = "errUNKNOWN";
            break;
        case DFU_STATUS_ERROR_STALLEDPKT:
            message = "errSTALLEDPKT";
            break;
    }
    return message;
}

int32_t dfu_make_idle( dfu_device_t *device, const dfu_bool initial_abort ) {
    dfu_status_t status;
    int32_t retries = 4;

    if( true == initial_abort ) {
        dfu_abort( device );
    }

    while( 0 < retries ) {
        if( 0 != dfu_get_status(device, &status) ) {
            dfu_clear_status( device );
            continue;
        }

        DEBUG( "State: %s (%d)\n", dfu_state_to_string(status.bState), status.bState );

        switch( status.bState ) {
            case STATE_DFU_IDLE:
                if( DFU_STATUS_OK == status.bStatus ) {
                    return 0;
                }

                /* We need the device to have the DFU_STATUS_OK status. */
                dfu_clear_status( device );
                break;

            case STATE_DFU_DOWNLOAD_SYNC:   /* abort -> idle */
            case STATE_DFU_DOWNLOAD_IDLE:   /* abort -> idle */
            case STATE_DFU_MANIFEST_SYNC:   /* abort -> idle */
            case STATE_DFU_UPLOAD_IDLE:     /* abort -> idle */
            case STATE_DFU_DOWNLOAD_BUSY:   /* abort -> error */
            case STATE_DFU_MANIFEST:        /* abort -> error */
                dfu_abort( device );
                break;

            case STATE_DFU_ERROR:
                dfu_clear_status( device );
                break;

            case STATE_APP_IDLE:
                dfu_detach( device, DFU_DETACH_TIMEOUT );
                break;

            case STATE_APP_DETACH:
            case STATE_DFU_MANIFEST_WAIT_RESET:
                DEBUG( "Resetting the device\n" );
                libusb_reset_device( device->handle );
                return 1;
        }

        retries--;
    }

    DEBUG( "Not able to transition the device into the dfuIDLE state.\n" );
    return -2;
}

int32_t dfu_transfer_out( dfu_device_t *device,
                          uint8_t request,
                          const int32_t value,
                          uint8_t* data,
                          const size_t length ) {
    return libusb_control_transfer( device->handle,
                /* bmRequestType */ LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
                /* bRequest      */ request,
                /* wValue        */ value,
                /* wIndex        */ device->interface,
                /* Data          */ data,
                /* wLength       */ length,
                                    DFU_TIMEOUT );
}

int32_t dfu_transfer_in( dfu_device_t *device,
                         uint8_t request,
                         const int32_t value,
                         uint8_t* data,
                         const size_t length ) {
    return libusb_control_transfer( device->handle,
                /* bmRequestType */ LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
                /* bRequest      */ request,
                /* wValue        */ value,
                /* wIndex        */ device->interface,
                /* Data          */ data,
                /* wLength       */ length,
                                    DFU_TIMEOUT );
}

void dfu_msg_response_output( const char *function, const int32_t result ) {
    char *msg = NULL;

    if( 0 <= result ) {
        msg = "No error.";
    } else {
        switch( result ) {
            case LIBUSB_ERROR_IO :
                msg = "LIBUSB_ERROR_IO: Input/output error.";
                break;
            case LIBUSB_ERROR_INVALID_PARAM :
                msg = "LIBUSB_ERROR_INVALID_PARAM: Invalid parameter.";
                break;
            case LIBUSB_ERROR_ACCESS :
                msg = "LIBUSB_ERROR_ACCESS: Access denied (insufficient permissions)";
                break;
            case LIBUSB_ERROR_NO_DEVICE :
                msg = "LIBUSB_ERROR_NO_DEVICE: No such device (it may have been disconnected)";
                break;
            case LIBUSB_ERROR_NOT_FOUND :
                msg = "LIBUSB_ERROR_NOT_FOUND: Entity not found.";
                break;
            case LIBUSB_ERROR_BUSY :
                msg = "LIBUSB_ERROR_BUSY: Resource busy.";
                break;
            case LIBUSB_ERROR_TIMEOUT :
                msg = "LIBUSB_ERROR_TIMEOUT: Operation timed out.";
                break;
            case LIBUSB_ERROR_OVERFLOW :
                msg = "LIBUSB_ERROR_OVERFLOW: Overflow.";
                break;
            case LIBUSB_ERROR_PIPE :
                msg = "LIBUSB_ERROR_PIPE: Pipe error.";
                break;
            case LIBUSB_ERROR_INTERRUPTED :
                msg = "LIBUSB_ERROR_INTERRUPTED: System call interrupted (perhaps due to signal)";
                break;
            case LIBUSB_ERROR_NO_MEM :
                msg = "LIBUSB_ERROR_NO_MEM: Insufficient memory.";
                break;
            case LIBUSB_ERROR_NOT_SUPPORTED :
                msg = "LIBUSB_ERROR_NOT_SUPPORTED: Operation not supported or unimplemented on this platform.";
                break;
            case LIBUSB_ERROR_OTHER :
                msg = "LIBUSB_ERROR_OTHER: Other error.";
                break;
            default:
                msg = "Unknown error";
                break;
        }
        DEBUG( "%s ERR: %s 0x%08x (%d)\n", function, msg, result, result );
    }
}

#ifdef malloc
#undef malloc
void* rpl_malloc( size_t n ) {
    if( 0 == n ) {
        n = 1;
    }

    return malloc( n );
}
#endif
