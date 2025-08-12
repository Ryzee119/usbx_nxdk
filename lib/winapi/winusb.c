// SPDX-License-Identifier: MIT

// SPDX-FileCopyrightText: 2024 Ryan Wendland

#include <assert.h>
#include <limits.h>
#include <nxdk/usb.h>
#include <stdlib.h>
#include <windows.h>
#include <winusb.h>

#include "ux_api.h"
#include "ux_hcd_ohci.h"
#include "ux_host_stack.h"

static DWORD ux_status_to_win32 (UINT status);
#define LOCK_INTERFACE(a)   nxUsbLock(a->nx_device);
#define UNLOCK_INTERFACE(a) nxUsbUnlock(a->nx_device);

#define GET_UX_INTERFACE(winusb_interface)     ((winusb_interface)->ux_interface)
#define GET_UX_CONFIGURATION(winusb_interface) ((winusb_interface)->ux_interface->ux_interface_configuration)
#define GET_UX_DEVICE(winusb_interface)        ((winusb_interface)->nx_device->ux_device);
#define RETURN_ON_INVALID_HANDLE(handle_check) \
    if (handle_check) {                        \
        SetLastError(ERROR_INVALID_HANDLE);    \
        return FALSE;                          \
    }

#define RETURN_ON_INVALID_PARAMETER(param_check) \
    if (param_check) {                           \
        SetLastError(ERROR_INVALID_PARAMETER);   \
        return FALSE;                            \
    }

#define RETURN_ON_DISCONNECTED_DEVICE(winusb_interface)   \
    if (interface_connected(winusb_interface) == FALSE) { \
        SetLastError(ERROR_DEV_NOT_EXIST);                \
        UNLOCK_INTERFACE(winusb_interface);               \
        return FALSE;                                     \
    }

typedef struct WINUSB_INTERFACE_STRUCT
{
    nx_usb_device_t *nx_device;
    UX_INTERFACE *ux_interface;
} WINUSB_INTERFACE_STRUCT;

static BOOL interface_connected (WINUSB_INTERFACE_HANDLE InterfaceHandle)
{
    WINUSB_INTERFACE_STRUCT *winusb_interface = (WINUSB_INTERFACE_STRUCT *)InterfaceHandle;
    return winusb_interface->nx_device->ux_device != NULL;
}

static UX_ENDPOINT *get_endpoint_from_pipeid (UX_INTERFACE *ux_interface, UCHAR PipeID)
{
    UX_ENDPOINT *endpoint = ux_interface->ux_interface_first_endpoint;
    while (endpoint != NULL) {
        if (endpoint->ux_endpoint_descriptor.bEndpointAddress == PipeID) {
            break;
        }
        endpoint = endpoint->ux_endpoint_next_endpoint;
    }
    return endpoint;
}

BOOL WinUsb_Initialize (HANDLE DeviceHandle, PWINUSB_INTERFACE_HANDLE InterfaceHandle)
{
    WINUSB_INTERFACE_STRUCT *winusb_interface = NULL;
    DWORD last_error;
    UINT status;

    RETURN_ON_INVALID_HANDLE(DeviceHandle == NULL);
    RETURN_ON_INVALID_HANDLE(InterfaceHandle == NULL);

    nx_usb_device_t *nx_device = (nx_usb_device_t *)DeviceHandle;

    nxUsbLock(nx_device);

    if (nx_device->ux_device == NULL) {
        last_error = ERROR_DEV_NOT_EXIST;
        goto init_fail;
    }

    UX_DEVICE *ux_device = nx_device->ux_device;

    // WinUsb only supports the first configuration.
    // For most USB devices, the first configuration is all we need.
    UX_CONFIGURATION *ux_configuration;
    status = ux_host_stack_device_configuration_get(ux_device, 0, &ux_configuration);
    if (status != UX_SUCCESS) {
        last_error = ux_status_to_win32(status);
        goto init_fail;
    }

    // USBX won't configure the device if there is no registered class driver. If the device is not configured,
    // configure it now. This allocates memory for endpoints and transfers.
    if (ux_device->ux_device_state != UX_DEVICE_CONFIGURED) {
        status = ux_host_stack_device_configuration_select(ux_configuration);
        if (status != UX_SUCCESS) {
            last_error = ux_status_to_win32(status);
            goto init_fail;
        }
    }

    // WinUSB always returns the first interface on the default alternate setting of the device.
    // If an application wants to use another interface on the device, it must call WinUsb_GetAssociatedInterface().
    // If you want to use an alternate interface setting, call WinUsb_SetCurrentAlternateSetting().
    UX_INTERFACE *ux_interface;
    status = ux_host_stack_configuration_interface_get(ux_configuration, 0, 0, &ux_interface);
    if (status != UX_SUCCESS) {
        last_error = ux_status_to_win32(status);
        goto init_fail;
    }

    // We use this spare padding byte to indicate that we have claimed this interface
    if (ux_interface->ux_interface_descriptor._align_size[0] == 1) {
        last_error = ERROR_ALREADY_EXISTS;
        return FALSE;
    }

    winusb_interface = malloc(sizeof(WINUSB_INTERFACE_STRUCT));
    if (winusb_interface == NULL) {
        last_error = ERROR_NOT_ENOUGH_MEMORY;
        goto init_fail;
    }

    // Mark as claimed and populate the interface structure
    ux_interface->ux_interface_descriptor._align_size[0] = 1;
    winusb_interface->nx_device = nx_device;
    winusb_interface->ux_interface = ux_interface;

    nxUsbUnlock(nx_device);

    *InterfaceHandle = (WINUSB_INTERFACE_HANDLE)winusb_interface;
    return TRUE;
init_fail:
    nxUsbUnlock(nx_device);
    if (winusb_interface) {
        free(winusb_interface);
    }
    SetLastError(last_error);
    return FALSE;
}

BOOL WinUsb_Free (WINUSB_INTERFACE_HANDLE InterfaceHandle)
{
    WINUSB_INTERFACE_STRUCT *winusb_interface = (WINUSB_INTERFACE_STRUCT *)InterfaceHandle;

    RETURN_ON_INVALID_HANDLE(InterfaceHandle == NULL);

    LOCK_INTERFACE(winusb_interface);
    winusb_interface->ux_interface->ux_interface_descriptor._align_size[0] = 0;
    winusb_interface->nx_device = NULL;
    winusb_interface->ux_interface = NULL;
    UNLOCK_INTERFACE(winusb_interface);
    free(winusb_interface);
    return TRUE;
}

BOOL WinUsb_SetCurrentAlternateSetting (WINUSB_INTERFACE_HANDLE InterfaceHandle, UCHAR SettingNumber)
{
    WINUSB_INTERFACE_STRUCT *winusb_interface = (WINUSB_INTERFACE_STRUCT *)InterfaceHandle;
    UINT status;

    RETURN_ON_INVALID_HANDLE(InterfaceHandle == NULL);
    LOCK_INTERFACE(winusb_interface);
    RETURN_ON_DISCONNECTED_DEVICE(winusb_interface);

    UX_INTERFACE *ux_interface = GET_UX_INTERFACE(winusb_interface);
    UX_CONFIGURATION *ux_configuration = GET_UX_CONFIGURATION(winusb_interface);
    UINT interface_index = ux_interface->ux_interface_descriptor.bAlternateSetting;

    status = ux_host_stack_configuration_interface_get(ux_configuration, interface_index, SettingNumber, &ux_interface);
    if (status != UX_SUCCESS) {
        SetLastError(ux_status_to_win32(status));
        UNLOCK_INTERFACE(winusb_interface);
        return FALSE;
    }

    status = ux_host_stack_interface_setting_select(ux_interface) != UX_SUCCESS;
    if (status != UX_SUCCESS) {
        SetLastError(ux_status_to_win32(status));
        UNLOCK_INTERFACE(winusb_interface);
        return FALSE;
    }

    UNLOCK_INTERFACE(winusb_interface);
    return TRUE;
}

BOOL WinUsb_GetCurrentAlternateSetting (WINUSB_INTERFACE_HANDLE InterfaceHandle, PUCHAR SettingNumber)
{
    WINUSB_INTERFACE_STRUCT *winusb_interface = (WINUSB_INTERFACE_STRUCT *)InterfaceHandle;
    BOOL status = FALSE;

    RETURN_ON_INVALID_HANDLE(InterfaceHandle == NULL);
    RETURN_ON_INVALID_PARAMETER(SettingNumber == NULL);

    LOCK_INTERFACE(winusb_interface);

    if (interface_connected(winusb_interface)) {
        UX_INTERFACE *ux_interface = GET_UX_INTERFACE(winusb_interface);
        *SettingNumber = ux_interface->ux_interface_descriptor.bAlternateSetting;
        status = TRUE;
    } else {
        SetLastError(ERROR_DEV_NOT_EXIST);
    }

    UNLOCK_INTERFACE(winusb_interface);
    return status;
}

BOOL WinUsb_GetAssociatedInterface (WINUSB_INTERFACE_HANDLE InterfaceHandle, UCHAR AssociatedInterfaceIndex,
                                    PWINUSB_INTERFACE_HANDLE AssociatedInterfaceHandle)
{
    WINUSB_INTERFACE_STRUCT *winusb_interface = (WINUSB_INTERFACE_STRUCT *)InterfaceHandle;
    UX_INTERFACE *ux_associated_interface;
    UINT status;

    RETURN_ON_INVALID_HANDLE(InterfaceHandle == NULL);
    RETURN_ON_INVALID_PARAMETER(AssociatedInterfaceHandle == NULL);
    LOCK_INTERFACE(winusb_interface);
    RETURN_ON_DISCONNECTED_DEVICE(winusb_interface);

    UX_CONFIGURATION *ux_configuration = GET_UX_CONFIGURATION(winusb_interface);

    // Retrieve the associated interface
    status = _ux_host_stack_configuration_interface_get(ux_configuration, AssociatedInterfaceIndex, 0,
                                                        &ux_associated_interface);
    if (status != UX_SUCCESS) {
        SetLastError(ux_status_to_win32(status));
        UNLOCK_INTERFACE(winusb_interface);
        return FALSE;
    }

    // Check if the associated interface is already in use
    if (ux_associated_interface->ux_interface_descriptor._align_size[0] != 0) {
        SetLastError(ERROR_ALREADY_EXISTS);
        UNLOCK_INTERFACE(winusb_interface);
        return FALSE;
    }

    // Allocate memory for the associated interface structure
    WINUSB_INTERFACE_STRUCT *winusb_associated_interface = malloc(sizeof(WINUSB_INTERFACE_STRUCT));
    if (!winusb_associated_interface) {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        UNLOCK_INTERFACE(winusb_interface);
        return FALSE;
    }

    // Initialize the associated interface structure
    winusb_associated_interface->nx_device = winusb_interface->nx_device;
    winusb_associated_interface->ux_interface = ux_associated_interface;

    // Mark as claimed
    winusb_associated_interface->ux_interface->ux_interface_descriptor._align_size[0] = 1;
    *AssociatedInterfaceHandle = (WINUSB_INTERFACE_HANDLE)winusb_associated_interface;

    // Unlock the interface and return the status
    UNLOCK_INTERFACE(winusb_interface);
    return TRUE;
}

BOOL WinUsb_QueryDeviceInformation (WINUSB_INTERFACE_HANDLE InterfaceHandle, ULONG InformationType, PULONG BufferLength,
                                    PVOID Buffer)
{
    WINUSB_INTERFACE_STRUCT *winusb_interface = (WINUSB_INTERFACE_STRUCT *)InterfaceHandle;

    if (BufferLength == NULL || Buffer == NULL || *BufferLength == 0) {
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return FALSE;
    }

    RETURN_ON_INVALID_PARAMETER(InformationType != DEVICE_SPEED);
    LOCK_INTERFACE(winusb_interface);
    RETURN_ON_DISCONNECTED_DEVICE(winusb_interface);

    ULONG ux_device_speed = GET_UX_CONFIGURATION(winusb_interface)->ux_configuration_device->ux_device_speed;
    UCHAR *pdevice_speed = Buffer;

    switch (ux_device_speed) {
        case UX_HIGH_SPEED_DEVICE:
            *pdevice_speed = HighSpeed;
            break;
        case UX_FULL_SPEED_DEVICE:
            *pdevice_speed = FullSpeed;
            break;
        case UX_LOW_SPEED_DEVICE:
            *pdevice_speed = LowSpeed;
            break;
        default:
            SetLastError(ERROR_INVALID_PARAMETER);
            UNLOCK_INTERFACE(winusb_interface);
            return FALSE;
            break;
    }

    UNLOCK_INTERFACE(winusb_interface);
    return TRUE;
}

BOOL WinUsb_ControlTransfer (WINUSB_INTERFACE_HANDLE InterfaceHandle, WINUSB_SETUP_PACKET SetupPacket, PUCHAR Buffer,
                             ULONG BufferLength, PULONG LengthTransferred, LPOVERLAPPED Overlapped)
{
    WINUSB_INTERFACE_STRUCT *winusb_interface = (WINUSB_INTERFACE_STRUCT *)InterfaceHandle;
    UINT status;

    // FIXME: Async control transfers not implemented
    assert(Overlapped == NULL);
    if (Overlapped != NULL) {
        SetLastError(ERROR_NOT_SUPPORTED);
        return FALSE;
    }

    LOCK_INTERFACE(winusb_interface);
    RETURN_ON_DISCONNECTED_DEVICE(winusb_interface);

    UX_DEVICE *ux_device = GET_UX_DEVICE(winusb_interface);
    UX_ENDPOINT *control_endpoint = &ux_device->ux_device_control_endpoint;
    UX_TRANSFER *ux_transfer = &control_endpoint->ux_endpoint_transfer_request;

    if (ux_transfer->ux_transfer_request_completion_code == UX_TRANSFER_STATUS_PENDING) {
        SetLastError(ERROR_BUSY);
        UNLOCK_INTERFACE(winusb_interface);
        return FALSE;
    }

    ux_transfer->ux_transfer_request_completion_code = UX_TRANSFER_STATUS_PENDING;

    assert(ux_transfer->ux_transfer_request_semaphore.semaphore);
    assert(ux_transfer->ux_transfer_request_semaphore.tx_semaphore_id == 0x53454D41);
    assert(ux_transfer->ux_transfer_request_semaphore.tx_semaphore_count == 0);

    // Initialize the transfer request structure
    ux_transfer->ux_transfer_request_endpoint = control_endpoint;
    ux_transfer->ux_transfer_request_data_pointer = Buffer;
    ux_transfer->ux_transfer_request_requested_length = UX_MIN(BufferLength, SetupPacket.Length);
    ux_transfer->ux_transfer_request_function = SetupPacket.Request;
    ux_transfer->ux_transfer_request_type = SetupPacket.RequestType;
    ux_transfer->ux_transfer_request_index = SetupPacket.Index;
    ux_transfer->ux_transfer_request_value = SetupPacket.Value;
    UNLOCK_INTERFACE(winusb_interface);

    status = ux_host_stack_transfer_request(ux_transfer);
    if (status != UX_SUCCESS) {
        SetLastError(ux_status_to_win32(status));
        return FALSE;
    }

    if (LengthTransferred) {
        *LengthTransferred = ux_transfer->ux_transfer_request_actual_length;
    }

    return TRUE;
}

BOOL WinUsb_GetDescriptor (WINUSB_INTERFACE_HANDLE InterfaceHandle, UCHAR DescriptorType, UCHAR Index,
                           USHORT LanguageID, PUCHAR Buffer, ULONG BufferLength, PULONG LengthTransferred)
{
    WINUSB_INTERFACE_STRUCT *winusb_interface = (WINUSB_INTERFACE_STRUCT *)InterfaceHandle;
    BOOL got_cached_descriptor = FALSE;
    DWORD last_error = ERROR_SUCCESS;
    WINUSB_SETUP_PACKET setup_packet;

    LOCK_INTERFACE(winusb_interface);
    RETURN_ON_DISCONNECTED_DEVICE(winusb_interface);

    UX_DEVICE *ux_device = GET_UX_DEVICE(winusb_interface);
    UX_CONFIGURATION *ux_configuration = GET_UX_CONFIGURATION(winusb_interface);

    // Handle the request based on the DescriptorType
    switch (DescriptorType) {
        case USB_DEVICE_DESCRIPTOR_TYPE:
            if (BufferLength < sizeof(UX_DEVICE_DESCRIPTOR)) {
                last_error = ERROR_INSUFFICIENT_BUFFER;
            } else {
                memcpy(Buffer, &ux_device->ux_device_descriptor, sizeof(UX_DEVICE_DESCRIPTOR));
                *LengthTransferred = sizeof(UX_DEVICE_DESCRIPTOR);
                got_cached_descriptor = TRUE;
            }
            break;

        case USB_CONFIGURATION_DESCRIPTOR_TYPE:
            if (BufferLength < sizeof(UX_CONFIGURATION_DESCRIPTOR)) {
                last_error = ERROR_INSUFFICIENT_BUFFER;
            } else if (BufferLength == sizeof(UX_CONFIGURATION_DESCRIPTOR)) {
                memcpy(Buffer, &ux_configuration->ux_configuration_descriptor, sizeof(UX_CONFIGURATION_DESCRIPTOR));
                *LengthTransferred = sizeof(UX_CONFIGURATION_DESCRIPTOR);
                got_cached_descriptor = TRUE;
            }
            break;

        case USB_STRING_DESCRIPTOR_TYPE:
            if (BufferLength < 2) {
                last_error = ERROR_INSUFFICIENT_BUFFER;
            } else if (BufferLength % 2 != 0) {
                last_error = ERROR_INVALID_PARAMETER;
            }
            break;

        default:
            last_error = ERROR_INVALID_PARAMETER;
            break;
    }

    UNLOCK_INTERFACE(winusb_interface);

    if (last_error == ERROR_SUCCESS) {
        // We are done
        if (got_cached_descriptor) {
            return TRUE;
        }

        // Fetch the descriptor from the device
        const REQUEST_TYPE RequestType = {
            .Bits.Recipient = BMREQUEST_TO_DEVICE,
            .Bits.Reserved = 0,
            .Bits.Type = BMREQUEST_STANDARD,
            .Bits.Dir = BMREQUEST_DEVICE_TO_HOST,
        };
        setup_packet.RequestType = RequestType.Byte;
        setup_packet.Request = USB_REQUEST_GET_DESCRIPTOR;
        setup_packet.Value = USB_DESCRIPTOR_MAKE_TYPE_AND_INDEX(DescriptorType, Index);
        setup_packet.Index = LanguageID;
        setup_packet.Length = BufferLength;

        return WinUsb_ControlTransfer(InterfaceHandle, setup_packet, Buffer, BufferLength, LengthTransferred, NULL);
    } else {
        SetLastError(last_error);
        return FALSE;
    }
}

BOOL WinUsb_QueryInterfaceSettings (WINUSB_INTERFACE_HANDLE InterfaceHandle, UCHAR AlternateInterfaceNumber,
                                    PUSB_INTERFACE_DESCRIPTOR UsbAltInterfaceDescriptor)
{
    WINUSB_INTERFACE_STRUCT *winusb_interface = (WINUSB_INTERFACE_STRUCT *)InterfaceHandle;

    // CHECK: Windows will crash with NULL descriptor. We want to atleast catch it with an assert
    assert(UsbAltInterfaceDescriptor);

    RETURN_ON_INVALID_HANDLE(InterfaceHandle == NULL);
    LOCK_INTERFACE(winusb_interface);
    RETURN_ON_DISCONNECTED_DEVICE(winusb_interface);

    UX_INTERFACE *ux_interface = GET_UX_INTERFACE(winusb_interface);
    UX_CONFIGURATION *ux_configuration = GET_UX_CONFIGURATION(winusb_interface);

    UCHAR bInterfaceNumber = ux_interface->ux_interface_descriptor.bInterfaceNumber;
    ux_interface = ux_configuration->ux_configuration_first_interface;
    while (ux_interface != UX_NULL) {
        if (ux_interface->ux_interface_descriptor.bInterfaceNumber == bInterfaceNumber &&
            ux_interface->ux_interface_descriptor.bAlternateSetting == AlternateInterfaceNumber) {
            memcpy(UsbAltInterfaceDescriptor, &ux_interface->ux_interface_descriptor, sizeof(USB_INTERFACE_DESCRIPTOR));
            UNLOCK_INTERFACE(winusb_interface);
            return TRUE;
        }

        ux_interface = ux_interface->ux_interface_next_interface;
    }

    SetLastError(ERROR_NO_MORE_ITEMS);
    UNLOCK_INTERFACE(winusb_interface);
    return FALSE;
}

BOOL WinUsb_QueryPipe (WINUSB_INTERFACE_HANDLE InterfaceHandle, UCHAR AlternateInterfaceNumber, UCHAR PipeIndex,
                       PWINUSB_PIPE_INFORMATION PipeInformation)
{
    WINUSB_INTERFACE_STRUCT *winusb_interface = (WINUSB_INTERFACE_STRUCT *)InterfaceHandle;
    UINT status;

    RETURN_ON_INVALID_HANDLE(InterfaceHandle == NULL);
    RETURN_ON_INVALID_PARAMETER(PipeInformation == NULL);
    LOCK_INTERFACE(winusb_interface);
    RETURN_ON_DISCONNECTED_DEVICE(winusb_interface);

    UX_INTERFACE *ux_interface = GET_UX_INTERFACE(winusb_interface);
    UX_CONFIGURATION *ux_configuration = GET_UX_CONFIGURATION(winusb_interface);

    UCHAR bInterfaceNumber = ux_interface->ux_interface_descriptor.bInterfaceNumber;
    ux_interface = ux_configuration->ux_configuration_first_interface;
    while (ux_interface != UX_NULL) {
        if (ux_interface->ux_interface_descriptor.bInterfaceNumber == bInterfaceNumber &&
            ux_interface->ux_interface_descriptor.bAlternateSetting == AlternateInterfaceNumber) {
            break;
        }

        ux_interface = ux_interface->ux_interface_next_interface;
    }

    if (ux_interface == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        UNLOCK_INTERFACE(winusb_interface);
        return FALSE;
    }

    if (PipeIndex > ux_interface->ux_interface_descriptor.bNumEndpoints) {
        SetLastError(ERROR_NO_MORE_ITEMS);
        UNLOCK_INTERFACE(winusb_interface);
        return FALSE;
    }

    UX_ENDPOINT *endpoint;
    status = ux_host_stack_interface_endpoint_get(ux_interface, PipeIndex, &endpoint);
    if (status != UX_SUCCESS) {
        SetLastError(ERROR_NO_MORE_ITEMS);
        UNLOCK_INTERFACE(winusb_interface);
        return FALSE;
    }

    UX_ENDPOINT_DESCRIPTOR *dep = &endpoint->ux_endpoint_descriptor;
    PipeInformation->PipeType = dep->bmAttributes & USB_ENDPOINT_TYPE_MASK;
    PipeInformation->PipeId = dep->bEndpointAddress;
    PipeInformation->MaximumPacketSize = dep->wMaxPacketSize;
    PipeInformation->Interval = dep->bInterval;

    UNLOCK_INTERFACE(winusb_interface);
    return TRUE;
}

BOOL WinUsb_QueryPipeEx (WINUSB_INTERFACE_HANDLE InterfaceHandle, UCHAR AlternateInterfaceNumber, UCHAR PipeIndex,
                         PWINUSB_PIPE_INFORMATION_EX PipeInformationEx)
{
    // We dont support high speed and super speed so the maximum bytes per interval is the same as the maximum packet
    // size. We just call WinUsb_QueryPipe then populate the extra field in the extended structure manually.
    if (WinUsb_QueryPipe(InterfaceHandle, AlternateInterfaceNumber, PipeIndex,
                         (PWINUSB_PIPE_INFORMATION)PipeInformationEx)) {
        PipeInformationEx->MaximumBytesPerInterval = PipeInformationEx->MaximumPacketSize;
        return TRUE;
    }
    return FALSE;
}

static void transfer_request_completion_function (UX_TRANSFER *ux_transfer)
{
    LPOVERLAPPED lpOverlapped = (LPOVERLAPPED)ux_transfer->ux_transfer_request_user_specific;
    assert(lpOverlapped);

    // In a win32 overlapped struct, InternalHigh contains the actual bytes transferred and Internal contains the
    // transfer status
    lpOverlapped->InternalHigh = ux_transfer->ux_transfer_request_actual_length;
    if (ux_transfer->ux_transfer_request_completion_code == UX_SUCCESS) {
        lpOverlapped->Internal = STATUS_SUCCESS;
    } else {
        lpOverlapped->Internal = STATUS_UNSUCCESSFUL;
    }

    // On aborted transfers also set the transfer semaphore as USBX doesn't do it
    if (ux_transfer->ux_transfer_request_completion_code == UX_TRANSFER_STATUS_ABORT) {
        _ux_host_semaphore_put(&ux_transfer->ux_transfer_request_semaphore);
    }

    // Let user know we are done
    SetEvent(lpOverlapped->hEvent);
}

BOOL WinUsb_ReadPipe (WINUSB_INTERFACE_HANDLE InterfaceHandle, UCHAR PipeID, PUCHAR Buffer, ULONG BufferLength,
                      PULONG LengthTransferred, LPOVERLAPPED Overlapped)
{
    WINUSB_INTERFACE_STRUCT *winusb_interface = (WINUSB_INTERFACE_STRUCT *)InterfaceHandle;
    BOOL is_synchronous = (Overlapped == NULL);
    UINT status;

    RETURN_ON_INVALID_HANDLE(InterfaceHandle == NULL);
    LOCK_INTERFACE(winusb_interface);
    RETURN_ON_DISCONNECTED_DEVICE(winusb_interface);

    UX_INTERFACE *ux_interface = GET_UX_INTERFACE(winusb_interface);
    UX_ENDPOINT *ux_endpoint = get_endpoint_from_pipeid(ux_interface, PipeID);
    if (ux_endpoint == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        UNLOCK_INTERFACE(winusb_interface);
        return FALSE;
    }

    UX_TRANSFER *ux_transfer = &ux_endpoint->ux_endpoint_transfer_request;
    if (ux_transfer->ux_transfer_request_completion_code == UX_TRANSFER_STATUS_PENDING) {
        SetLastError(ERROR_BUSY);
        UNLOCK_INTERFACE(winusb_interface);
        return FALSE;
    }

    assert(ux_transfer);
    assert(ux_transfer->ux_transfer_request_semaphore.semaphore);
    assert(ux_transfer->ux_transfer_request_semaphore.tx_semaphore_id == 0x53454D41);
    assert(ux_transfer->ux_transfer_request_semaphore.tx_semaphore_count == 0);

    ux_transfer->ux_transfer_request_endpoint = ux_endpoint;
    ux_transfer->ux_transfer_request_type = USB_ENDPOINT_DIRECTION_IN(PipeID) ? UX_REQUEST_IN : UX_REQUEST_OUT;
    ux_transfer->ux_transfer_request_requested_length = BufferLength;
    ux_transfer->ux_transfer_request_data_pointer = Buffer;
    if (is_synchronous) {
        ux_transfer->ux_transfer_request_completion_function = NULL;
        ux_transfer->ux_transfer_request_user_specific = NULL;
    } else {
        Overlapped->Internal = STATUS_PENDING;
        Overlapped->OffsetHigh = (DWORD)&ux_transfer->ux_transfer_request_semaphore;
        ux_transfer->ux_transfer_request_completion_function = transfer_request_completion_function;
        ux_transfer->ux_transfer_request_user_specific = Overlapped;
    }

    status = ux_host_stack_transfer_request(ux_transfer);
    UNLOCK_INTERFACE(winusb_interface);

    if (status == UX_SUCCESS) {
        if (is_synchronous) {
            // Block until the transfer is completed (success, aborted, timeout)
            ux_utility_semaphore_get(&ux_transfer->ux_transfer_request_semaphore, UX_WAIT_FOREVER);
            status = ux_transfer->ux_transfer_request_completion_code;

            if (status == UX_SUCCESS && LengthTransferred) {
                *LengthTransferred = ux_transfer->ux_transfer_request_actual_length;
            } else if (status != UX_SUCCESS) {
                SetLastError(ux_status_to_win32(status));
            }

        } else {
            // Its happening in the background. We're done here
            SetLastError(ERROR_IO_PENDING);
            return FALSE;
        }
    } else {
        SetLastError(ux_status_to_win32(status));
    }

    return (status == UX_SUCCESS);
}

BOOL WinUsb_WritePipe (WINUSB_INTERFACE_HANDLE InterfaceHandle, UCHAR PipeID, PUCHAR Buffer, ULONG BufferLength,
                       PULONG LengthTransferred, LPOVERLAPPED Overlapped)
{
    // ReadPipe code handles writes too
    return WinUsb_ReadPipe(InterfaceHandle, PipeID, Buffer, BufferLength, LengthTransferred, Overlapped);
}

BOOL WinUsb_AbortPipe (WINUSB_INTERFACE_HANDLE InterfaceHandle, UCHAR PipeID)
{
    WINUSB_INTERFACE_STRUCT *winusb_interface = (WINUSB_INTERFACE_STRUCT *)InterfaceHandle;

    RETURN_ON_INVALID_HANDLE(InterfaceHandle == NULL);
    LOCK_INTERFACE(winusb_interface);
    RETURN_ON_DISCONNECTED_DEVICE(winusb_interface);

    UX_INTERFACE *ux_interface = GET_UX_INTERFACE(winusb_interface);
    UX_ENDPOINT *ux_endpoint = get_endpoint_from_pipeid(ux_interface, PipeID);
    ux_host_stack_endpoint_transfer_abort(ux_endpoint);

    UNLOCK_INTERFACE(winusb_interface);

    return TRUE;
}

BOOL WinUsb_FlushPipe (WINUSB_INTERFACE_HANDLE InterfaceHandle, UCHAR PipeID)
{
    // We don't buffer transfers so this is a no-op
    return TRUE;
}

BOOL WinUsb_ResetPipe (WINUSB_INTERFACE_HANDLE InterfaceHandle, UCHAR PipeID)
{
    WINUSB_INTERFACE_STRUCT *winusb_interface = (WINUSB_INTERFACE_STRUCT *)InterfaceHandle;
    UINT status;

    RETURN_ON_INVALID_HANDLE(InterfaceHandle == NULL);
    LOCK_INTERFACE(winusb_interface);
    RETURN_ON_DISCONNECTED_DEVICE(winusb_interface);

    UX_INTERFACE *ux_interface = GET_UX_INTERFACE(winusb_interface);
    UX_ENDPOINT *ux_endpoint = get_endpoint_from_pipeid(ux_interface, PipeID);
    if (ux_endpoint == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        UNLOCK_INTERFACE(winusb_interface);
        return FALSE;
    }

    status = _ux_host_stack_endpoint_reset(ux_endpoint);
    if (status != UX_SUCCESS) {
        SetLastError(ux_status_to_win32(status));
        UNLOCK_INTERFACE(winusb_interface);
        return FALSE;
    }

    UNLOCK_INTERFACE(winusb_interface);
    return TRUE;
}

BOOL WinUsb_GetCurrentFrameNumber (WINUSB_INTERFACE_HANDLE InterfaceHandle, PULONG CurrentFrameNumber,
                                   LARGE_INTEGER *TimeStamp)
{
    WINUSB_INTERFACE_STRUCT *winusb_interface = (WINUSB_INTERFACE_STRUCT *)InterfaceHandle;
    ULONG frame_number;
    UINT status = UX_ERROR;

    RETURN_ON_INVALID_HANDLE(InterfaceHandle == NULL);
    RETURN_ON_INVALID_PARAMETER(CurrentFrameNumber == NULL);
    RETURN_ON_INVALID_PARAMETER(TimeStamp == NULL);

    UX_INTERRUPT_SAVE_AREA
    UX_DISABLE

    UX_DEVICE *ux_device = GET_UX_DEVICE(winusb_interface);
    if (ux_device) {
        UX_HCD *hcd = UX_DEVICE_HCD_GET(ux_device);
        if (hcd) {
            QueryPerformanceCounter(TimeStamp);
            status = hcd->ux_hcd_entry_function(hcd, UX_HCD_GET_FRAME_NUMBER, (VOID *)&frame_number);
        }
    }

    UX_RESTORE

    if (status != UX_SUCCESS) {
        SetLastError(ux_status_to_win32(status));
        return FALSE;
    } else {
        *CurrentFrameNumber = frame_number;
        return TRUE;
    }
}

BOOL WinUsb_GetAdjustedFrameNumber (PULONG CurrentFrameNumber, LARGE_INTEGER TimeStamp)
{
    LARGE_INTEGER counter_frequency;
    LARGE_INTEGER current_counter;
    LARGE_INTEGER elapsed_time;
    LARGE_INTEGER elapsed_time_ms;

    RETURN_ON_INVALID_PARAMETER(CurrentFrameNumber == NULL);

    QueryPerformanceCounter(&current_counter);
    QueryPerformanceFrequency(&counter_frequency);

    // How many counts have occurred
    elapsed_time.QuadPart = current_counter.QuadPart - TimeStamp.QuadPart;

    // Convert to milliseconds
    // Prevent overflow when converting to milliseconds by changing order of division and multiplication
    // if we are going to overflow. FIXME: MulDiv64?
    if (elapsed_time.QuadPart < (LLONG_MAX / 1000)) {
        elapsed_time_ms.QuadPart = (elapsed_time.QuadPart * 1000) / counter_frequency.QuadPart;
    } else {
        elapsed_time_ms.QuadPart = (elapsed_time.QuadPart / counter_frequency.QuadPart) * 1000;
    }

    // When testing on windows, anything beyond 1000ms resulted in ERROR_INVALID_PARAMETER error. This is verified with
    // decomp and is what Windows does.
    if (elapsed_time_ms.QuadPart < 0 || elapsed_time_ms.QuadPart > 1000) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    *CurrentFrameNumber += elapsed_time_ms.LowPart;
    return TRUE;
}

BOOL WinUsb_GetOverlappedResult (WINUSB_INTERFACE_HANDLE InterfaceHandle, LPOVERLAPPED lpOverlapped,
                                 LPDWORD lpNumberOfBytesTransferred, BOOL bWait)
{
    // Windows documentation advises against using file handles for overlapped I/O on USB devices.
    // "Using an event object is better due to potential confusion with multiple concurrent overlapped operations on the
    // same file." While Windows supports this, the Xbox implementation does not support waiting on the file
    // handle. Therefore, we assert that the overlapped structure exists and we only wait on the event object.
    assert(lpOverlapped);
    assert(lpNumberOfBytesTransferred);
    RETURN_ON_INVALID_PARAMETER(lpOverlapped == NULL);
    RETURN_ON_INVALID_PARAMETER(lpOverlapped->hEvent == NULL);

    // Wait for the backend USBX semaphore to be signaled to signify the transfer is complete
    if (bWait == TRUE) {
        UX_SEMAPHORE *ux_semaphore = (UX_SEMAPHORE *)lpOverlapped->OffsetHigh;
        lpOverlapped->OffsetHigh = 0;
        ux_utility_semaphore_get(ux_semaphore, UX_WAIT_FOREVER);
    }

    // If bWait was true, the transfer is already completed by now so this should finish very clearly. This sets the
    // users event object
    BOOL status = GetOverlappedResult((HANDLE)InterfaceHandle, lpOverlapped, lpNumberOfBytesTransferred, bWait);

    return status;
}

#define MAX_QUEUED_ISO_TRANSFERS 2
typedef struct WINUSH_ISOCH_DATA
{
    UX_TRANSFER ux_transfer;
    PUSBD_ISO_PACKET_DESCRIPTOR IsoPacketDescriptors;
    ULONG NumberOfPackets;
    LPOVERLAPPED lpOverlapped;
} WINUSH_ISOCH_DATA;

typedef struct WINUSB_ISOCH_BUFFER_STRUCT
{
    WINUSB_INTERFACE_HANDLE InterfaceHandle;
    UCHAR PipeID;
    PUCHAR Buffer;
    ULONG BufferLength;
    WINUSH_ISOCH_DATA transfer_data[MAX_QUEUED_ISO_TRANSFERS]; // For double buffering
} WINUSB_ISOCH_BUFFER_STRUCT;

static void iso_transfer_request_completion_function (UX_TRANSFER *ux_transfer)
{
    WINUSH_ISOCH_DATA *transfer_data = (WINUSH_ISOCH_DATA *)ux_transfer->ux_transfer_request_user_specific;

    // Populate the WinUSB IsoPacketDescriptors packet status for iso reads
    if (ux_transfer->ux_transfer_request_type == UX_REQUEST_IN) {
        assert(transfer_data->IsoPacketDescriptors);
        ULONG bytes_transferred = transfer_data->ux_transfer.ux_transfer_request_actual_length;
        ULONG bytes_per_packet = transfer_data->ux_transfer.ux_transfer_request_packet_length;
        for (ULONG i = 0; i < transfer_data->NumberOfPackets; i++) {
            if (bytes_transferred >= bytes_per_packet) {
                transfer_data->IsoPacketDescriptors[i].Length = bytes_per_packet;
                transfer_data->IsoPacketDescriptors[i].Status = USBD_STATUS_SUCCESS;
                bytes_transferred -= bytes_per_packet;
            } else if (bytes_transferred > 0) {
                transfer_data->IsoPacketDescriptors[i].Length = bytes_transferred;
                transfer_data->IsoPacketDescriptors[i].Status = USBD_STATUS_BUFFER_UNDERRUN;
                bytes_transferred = 0;
            } else {
                transfer_data->IsoPacketDescriptors[i].Length = 0;
                transfer_data->IsoPacketDescriptors[i].Status = USBD_STATUS_NOT_ACCESSED;
            }
            transfer_data->IsoPacketDescriptors[i].Offset = i * bytes_per_packet;
        }
    }

    // https://learn.microsoft.com/en-us/windows-hardware/drivers/usbcon/getting-set-up-to-use-windows-devices-usb
    // For isochronous read and write transfers, the lpNumberOfBytesTransferred parameter is always 0
    // For a write transfer, the app assumes that if the operation completed successfully, all bytes
    // were transferred. For a read transfer, the Length member of each isochronous packet (USBD_ISO_PACKET_DESCRIPTOR),
    // contains the number bytes transferred in that packet, per interval. To get the total length, the app adds all
    // Length values.
    ux_transfer->ux_transfer_request_actual_length = 0;
    ux_transfer->ux_transfer_request_user_specific = transfer_data->lpOverlapped;

    // Call the normal completion function
    transfer_request_completion_function(ux_transfer);
}

BOOL WinUsb_RegisterIsochBuffer (WINUSB_INTERFACE_HANDLE InterfaceHandle, UCHAR PipeID, PUCHAR Buffer,
                                 ULONG BufferLength, PWINUSB_ISOCH_BUFFER_HANDLE IsochBufferHandle)
{
    WINUSB_INTERFACE_STRUCT *winusb_interface = (WINUSB_INTERFACE_STRUCT *)InterfaceHandle;

    RETURN_ON_INVALID_HANDLE(InterfaceHandle == NULL);

    if (Buffer == NULL || BufferLength == 0) {
        SetLastError(ERROR_INVALID_USER_BUFFER);
        return FALSE;
    }

    // Null InterfaceHandle crashes on Windows, lets catch it atleast
    assert(InterfaceHandle);

    LOCK_INTERFACE(winusb_interface);

    UX_INTERFACE *ux_interface = GET_UX_INTERFACE(winusb_interface);
    UX_ENDPOINT *ux_endpoint = get_endpoint_from_pipeid(ux_interface, PipeID);
    if (ux_endpoint == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        UNLOCK_INTERFACE(winusb_interface);
        return FALSE;
    }

    WINUSB_ISOCH_BUFFER_STRUCT *isoch_buffer = malloc(sizeof(WINUSB_ISOCH_BUFFER_STRUCT));
    if (isoch_buffer == NULL) {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        UNLOCK_INTERFACE(winusb_interface);
        return FALSE;
    }

    memset(isoch_buffer, 0x00, sizeof(WINUSB_ISOCH_BUFFER_STRUCT));
    isoch_buffer->InterfaceHandle = InterfaceHandle;
    isoch_buffer->PipeID = PipeID;
    isoch_buffer->Buffer = Buffer;
    isoch_buffer->BufferLength = BufferLength;

    // Prepare two transfer data structures for double buffering to prevent memory allocations during transfers
    // It's unlikely you will need more than 2 transfers queued at once.
    for (INT i = 0; i < MAX_QUEUED_ISO_TRANSFERS; i++) {
        UX_TRANSFER *ux_transfer = &isoch_buffer->transfer_data[i].ux_transfer;
        ux_transfer->ux_transfer_request_endpoint = ux_endpoint;
        ux_transfer->ux_transfer_request_type = USB_ENDPOINT_DIRECTION_IN(PipeID) ? UX_REQUEST_IN : UX_REQUEST_OUT;
        UINT status = ux_utility_semaphore_create(&ux_transfer->ux_transfer_request_semaphore, NULL, 0);
        if (status != UX_SUCCESS) {
            while (--i >= 0) {
                ux_utility_semaphore_delete(&isoch_buffer->transfer_data[i].ux_transfer.ux_transfer_request_semaphore);
            }
            SetLastError(ux_status_to_win32(status));
            UNLOCK_INTERFACE(winusb_interface);
            free(isoch_buffer);
            return FALSE;
        }
    }
    *IsochBufferHandle = isoch_buffer;
    UNLOCK_INTERFACE(winusb_interface);
    return TRUE;
}

BOOL WinUsb_ReadIsochPipeAsap (WINUSB_ISOCH_BUFFER_HANDLE BufferHandle, ULONG Offset, ULONG Length, BOOL ContinueStream,
                               ULONG NumberOfPackets, PUSBD_ISO_PACKET_DESCRIPTOR IsoPacketDescriptors,
                               LPOVERLAPPED Overlapped)
{
    (void)ContinueStream;
    RETURN_ON_INVALID_HANDLE(BufferHandle == NULL);
    RETURN_ON_INVALID_PARAMETER(Length == 0);
    RETURN_ON_INVALID_PARAMETER(NumberOfPackets == 0);

    // Quick sanity check here. It's possible some are in use and this may fail later but catch the obvious case early
    // OHCI technically supports 8 packets within one iso TD, but USBX does not support this and instead creates a new
    // TD per packet
    if (NumberOfPackets > UX_MAX_ISO_TD) {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return FALSE;
    }

    WINUSB_ISOCH_BUFFER_STRUCT *isoch_buffer = (WINUSB_ISOCH_BUFFER_STRUCT *)BufferHandle;
    WINUSB_INTERFACE_STRUCT *winusb_interface = (WINUSB_INTERFACE_STRUCT *)isoch_buffer->InterfaceHandle;
    WINUSH_ISOCH_DATA *transfer_data = NULL;
    UX_TRANSFER *ux_transfer = NULL;
    UINT status;

    // Check for buffer overflow or wrap around issues
    if ((Length + Offset) > isoch_buffer->BufferLength || (isoch_buffer->Buffer + Offset) < isoch_buffer->Buffer) {
        SetLastError(ERROR_INVALID_USER_BUFFER);
        return FALSE;
    }

    LOCK_INTERFACE(winusb_interface);
    RETURN_ON_DISCONNECTED_DEVICE(winusb_interface);

    // Find a free transfer data structure
    for (INT i = 0; i < MAX_QUEUED_ISO_TRANSFERS; i++) {
        ux_transfer = &isoch_buffer->transfer_data[i].ux_transfer;
        if (ux_transfer->ux_transfer_request_completion_code != UX_TRANSFER_STATUS_PENDING) {
            transfer_data = &isoch_buffer->transfer_data[i];
            break;
        }
    }
    if (transfer_data == NULL) {
        SetLastError(ERROR_BUSY);
        UNLOCK_INTERFACE(winusb_interface);
        return FALSE;
    }

    ux_transfer = &transfer_data->ux_transfer;
    ux_transfer->ux_transfer_request_requested_length = Length;
    ux_transfer->ux_transfer_request_data_pointer = isoch_buffer->Buffer + Offset;

    transfer_data->IsoPacketDescriptors = IsoPacketDescriptors;
    transfer_data->NumberOfPackets = NumberOfPackets;
    transfer_data->lpOverlapped = Overlapped;

    if (Overlapped) {
        Overlapped->Internal = STATUS_PENDING;
        ux_transfer->ux_transfer_request_completion_function = iso_transfer_request_completion_function;
        ux_transfer->ux_transfer_request_user_specific = transfer_data;
    } else {
        ux_transfer->ux_transfer_request_completion_function = NULL;
        ux_transfer->ux_transfer_request_user_specific = NULL;
    }

    // This function automatically splits transfers into chunks equal to the endpoints max packet size
    // this mimics WinUSB behaviour, although problematic if you want packets less than max packet. FIXME?
    status = ux_host_stack_transfer_request(ux_transfer);
    UNLOCK_INTERFACE(winusb_interface);

    BOOL is_synchronous = (Overlapped == NULL);
    if (status == UX_SUCCESS) {
        if (is_synchronous) {
            // Block until the transfer is completed (success, aborted, timeout)
            ux_utility_semaphore_get(&ux_transfer->ux_transfer_request_semaphore, UX_WAIT_FOREVER);
            status = ux_transfer->ux_transfer_request_completion_code;
            if (status != UX_SUCCESS) {
                ux_host_stack_transfer_request_abort(ux_transfer);
                SetLastError(ux_status_to_win32(status));
            }

        } else {
            // Its happening in the background. We're done here
            SetLastError(ERROR_IO_PENDING);
            return FALSE;
        }
    } else {
        SetLastError(ux_status_to_win32(status));
    }

    return (status == UX_SUCCESS);
}

BOOL WinUsb_WriteIsochPipeAsap (WINUSB_ISOCH_BUFFER_HANDLE BufferHandle, ULONG Offset, ULONG Length,
                                BOOL ContinueStream, LPOVERLAPPED Overlapped)
{
    // ReadIsoPipe code supports writes too, we just NULL out the read specific fields
    return WinUsb_ReadIsochPipeAsap(BufferHandle, Offset, Length, ContinueStream, 1, NULL, Overlapped);
}

BOOL WinUsb_UnregisterIsochBuffer (WINUSB_ISOCH_BUFFER_HANDLE IsochBufferHandle)
{
    RETURN_ON_INVALID_HANDLE(IsochBufferHandle == NULL);

    // Abort transfer
    //  Wait for complete
    // Free memory
    return TRUE;
}

BOOL WinUsb_GetPowerPolicy (WINUSB_INTERFACE_HANDLE InterfaceHandle, ULONG PolicyType, PULONG ValueLength, PVOID Value)
{
    BOOL status = TRUE;

    RETURN_ON_INVALID_HANDLE(InterfaceHandle == NULL);
    RETURN_ON_INVALID_PARAMETER(Value == NULL);
    RETURN_ON_INVALID_PARAMETER(ValueLength == NULL);

    // We don't currently support any automatic suspend policies so we return 0 to valid requests
    // In this function ERROR_INVALID_PARAMETER is returned when the buffer is insufficient
    switch (PolicyType) {
        case AUTO_SUSPEND:
            if (*ValueLength < 1) {
                status = FALSE;
            } else {
                *ValueLength = 1;
                *(PULONG)Value = 0;
            }
        case SUSPEND_DELAY:
            if (*ValueLength < 4) {
                status = FALSE;
            } else {
                *ValueLength = 4;
                *(PULONG)Value = 0;
            }
        default:
            status = FALSE;
    }
    if (status == FALSE) {
        SetLastError(ERROR_INVALID_PARAMETER);
    }
    return status;
}

BOOL WinUSB_EXT_OverWriteMaxPacketSize (WINUSB_INTERFACE_HANDLE InterfaceHandle, UCHAR PipeID,
                                        PUSHORT MaximumPacketSize)
{
    WINUSB_INTERFACE_STRUCT *winusb_interface = (WINUSB_INTERFACE_STRUCT *)InterfaceHandle;

    RETURN_ON_INVALID_HANDLE(InterfaceHandle == NULL);
    RETURN_ON_INVALID_PARAMETER(MaximumPacketSize == NULL);
    LOCK_INTERFACE(winusb_interface);
    RETURN_ON_DISCONNECTED_DEVICE(winusb_interface);

    UX_INTERFACE *ux_interface = GET_UX_INTERFACE(winusb_interface);
    UX_ENDPOINT *ux_endpoint = get_endpoint_from_pipeid(ux_interface, PipeID);
    if (ux_endpoint == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        UNLOCK_INTERFACE(winusb_interface);
        return FALSE;
    }

    USHORT old_max_packet_size = ux_endpoint->ux_endpoint_descriptor.wMaxPacketSize;
    ux_endpoint->ux_endpoint_descriptor.wMaxPacketSize = *MaximumPacketSize;
    *MaximumPacketSize = old_max_packet_size;
    UNLOCK_INTERFACE(winusb_interface);
    return TRUE;
}

#ifndef WINUSB_DISABLE_UNIMPLEMENTED_STUBS
BOOL WinUsb_ReadIsochPipe (WINUSB_ISOCH_BUFFER_HANDLE BufferHandle, ULONG Offset, ULONG Length, PULONG FrameNumber,
                           ULONG NumberOfPackets, PUSBD_ISO_PACKET_DESCRIPTOR IsoPacketDescriptors,
                           LPOVERLAPPED Overlapped)
{
    // It's difficult to schedule isochronous transfers to a specific USB frame with USBX. In almost all cases
    // WinUsb_ReadIsochPipeAsap will be sufficient
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}

BOOL WinUsb_WriteIsochPipe (WINUSB_ISOCH_BUFFER_HANDLE BufferHandle, ULONG Offset, ULONG Length, PULONG FrameNumber,
                            LPOVERLAPPED Overlapped)
{
    // It's difficult to schedule isochronous transfers to a specific USB frame with USBX. In almost all cases
    // WinUsb_WriteIsochPipeAsap will be sufficient
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}

BOOL WinUsb_SetPipePolicy (WINUSB_INTERFACE_HANDLE InterfaceHandle, UCHAR PipeID, ULONG PolicyType, ULONG ValueLength,
                           PVOID Value)
{
    // Not sure if all of this is possible with USBX
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}

BOOL WinUsb_GetPipePolicy (WINUSB_INTERFACE_HANDLE InterfaceHandle, UCHAR PipeID, ULONG PolicyType, PULONG ValueLength,
                           PVOID Value)
{
    // FIXME, we could atleast return some default valid values
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;

#if (0)
    // clang-format off
    // For reference, I ran this function against all endpoint types and the results are below
    Pipe: 00 : Control
    FAIL : type : SHORT_PACKET_TERMINATE error : ERROR_INVALID_PARAMETER
    FAIL : type : AUTO_CLEAR_STALL error : ERROR_INVALID_PARAMETER
    OKAY : type : PIPE_TRANSFER_TIMEOUT len : 4 val : 5000
    FAIL : type : IGNORE_SHORT_PACKETS error : ERROR_INVALID_PARAMETER
    FAIL : type : ALLOW_PARTIAL_READS error : ERROR_INVALID_PARAMETER
    FAIL : type : AUTO_FLUSH error : ERROR_INVALID_PARAMETER
    FAIL : type : RAW_IO error : ERROR_INVALID_PARAMETER
    OKAY : type : MAXIMUM_TRANSFER_SIZE len : 4 val : 0
    FAIL : type : RESET_PIPE_ON_RESUME error : ERROR_INVALID_PARAMETER

    Pipe : 80 : Control
    FAIL : type : SHORT_PACKET_TERMINATE error : ERROR_INVALID_PARAMETER
    FAIL : type : AUTO_CLEAR_STALL error : ERROR_INVALID_PARAMETER
    FAIL : type : PIPE_TRANSFER_TIMEOUT error : ERROR_INVALID_PARAMETER
    FAIL : type : IGNORE_SHORT_PACKETS error : ERROR_INVALID_PARAMETER
    FAIL : type : ALLOW_PARTIAL_READS error : ERROR_INVALID_PARAMETER
    FAIL : type : AUTO_FLUSH error : ERROR_INVALID_PARAMETER
    FAIL : type : RAW_IO error : ERROR_INVALID_PARAMETER
    FAIL : type : MAXIMUM_TRANSFER_SIZE error : ERROR_INVALID_PARAMETER
    FAIL : type : RESET_PIPE_ON_RESUME error : ERROR_INVALID_PARAMETER

    Pipe : 81 : Interrupt IN
    FAIL : type : SHORT_PACKET_TERMINATE error : ERROR_INVALID_PARAMETER
    OKAY : type : AUTO_CLEAR_STALL len : 1 val : 0
    OKAY : type : PIPE_TRANSFER_TIMEOUT len : 4 val : 0
    OKAY : type : IGNORE_SHORT_PACKETS len : 1 val : 0
    OKAY : type : ALLOW_PARTIAL_READS len : 1 val : 1
    OKAY : type : AUTO_FLUSH len : 1 val : 0
    OKAY : type : RAW_IO len : 1 val : 0
    OKAY : type : MAXIMUM_TRANSFER_SIZE len : 4 val : 262144
    OKAY : type : RESET_PIPE_ON_RESUME len : 1 val : 0

    Pipe : 02 : Interrupt OUT
    OKAY : type : SHORT_PACKET_TERMINATE len : 1 val : 0
    OKAY : type : AUTO_CLEAR_STALL len : 1 val : 0
    OKAY : type : PIPE_TRANSFER_TIMEOUT len : 4 val : 0
    FAIL : type : IGNORE_SHORT_PACKETS error : ERROR_INVALID_PARAMETER
    FAIL : type : ALLOW_PARTIAL_READS error : ERROR_INVALID_PARAMETER
    FAIL : type : AUTO_FLUSH error : ERROR_INVALID_PARAMETER
    OKAY : type : RAW_IO len : 1 val : 0
    OKAY : type : MAXIMUM_TRANSFER_SIZE len : 4 val : 262144
    OKAY : type : RESET_PIPE_ON_RESUME len : 1 val : 0

    Pipe : 83 : Bulk IN
    FAIL : type : SHORT_PACKET_TERMINATE error : ERROR_INVALID_PARAMETER
    OKAY : type : AUTO_CLEAR_STALL len : 1 val : 0
    OKAY : type : PIPE_TRANSFER_TIMEOUT len : 4 val : 0
    OKAY : type : IGNORE_SHORT_PACKETS len : 1 val : 0
    OKAY : type : ALLOW_PARTIAL_READS len : 1 val : 1
    OKAY : type : AUTO_FLUSH len : 1 val : 0
    OKAY : type : RAW_IO len : 1 val : 0
    OKAY : type : MAXIMUM_TRANSFER_SIZE len : 4 val : 262144
    OKAY : type : RESET_PIPE_ON_RESUME len : 1 val : 0

    Pipe : 04 : Bulk OUT
    OKAY : type : SHORT_PACKET_TERMINATE len : 1 val : 0
    OKAY : type : AUTO_CLEAR_STALL len : 1 val : 0
    OKAY : type : PIPE_TRANSFER_TIMEOUT len : 4 val : 0
    FAIL : type : IGNORE_SHORT_PACKETS error : ERROR_INVALID_PARAMETER
    FAIL : type : ALLOW_PARTIAL_READS error : ERROR_INVALID_PARAMETER
    FAIL : type : AUTO_FLUSH error : ERROR_INVALID_PARAMETER
    OKAY : type : RAW_IO len : 1 val : 0
    OKAY : type : MAXIMUM_TRANSFER_SIZE len : 4 val : 262144
    OKAY : type : RESET_PIPE_ON_RESUME len : 1 val : 0

    Pipe : 85 : Isochronous IN
    FAIL : type : SHORT_PACKET_TERMINATE error : ERROR_INVALID_PARAMETER
    FAIL : type : AUTO_CLEAR_STALL error : ERROR_INVALID_PARAMETER
    FAIL : type : PIPE_TRANSFER_TIMEOUT error : ERROR_INVALID_PARAMETER
    FAIL : type : IGNORE_SHORT_PACKETS error : ERROR_INVALID_PARAMETER
    FAIL : type : ALLOW_PARTIAL_READS error : ERROR_INVALID_PARAMETER
    FAIL : type : AUTO_FLUSH error : ERROR_INVALID_PARAMETER
    FAIL : type : RAW_IO error : ERROR_INVALID_PARAMETER
    OKAY : type : MAXIMUM_TRANSFER_SIZE len : 4 val : 262128
    FAIL : type : RESET_PIPE_ON_RESUME error : ERROR_INVALID_PARAMETER

    Pipe : 06 : Isochronous OUT
    FAIL : type : SHORT_PACKET_TERMINATE error : ERROR_INVALID_PARAMETER
    FAIL : type : AUTO_CLEAR_STALL error : ERROR_INVALID_PARAMETER
    FAIL : type : PIPE_TRANSFER_TIMEOUT error : ERROR_INVALID_PARAMETER
    FAIL : type : IGNORE_SHORT_PACKETS error : ERROR_INVALID_PARAMETER
    FAIL : type : ALLOW_PARTIAL_READS error : ERROR_INVALID_PARAMETER
    FAIL : type : AUTO_FLUSH error : ERROR_INVALID_PARAMETER
    FAIL : type : RAW_IO error : ERROR_INVALID_PARAMETER
    OKAY : type : MAXIMUM_TRANSFER_SIZE len : 4 val : 262128
    FAIL : type : RESET_PIPE_ON_RESUME error : ERROR_INVALID_PARAMETER
    // clang-format on
#endif
}

BOOL WinUsb_SetPowerPolicy (WINUSB_INTERFACE_HANDLE InterfaceHandle, ULONG PolicyType, ULONG ValueLength, PVOID Value)
{
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}
#endif

typedef struct
{
    UCHAR usbx_error;
    DWORD win32_error;
} UX_WIN32_ERROR_MAP;

static const UX_WIN32_ERROR_MAP error_map[] = {{UX_ERROR, ERROR_GEN_FAILURE},
                                               {UX_BUSY, ERROR_BUSY},
                                               {UX_TIMEOUT, ERROR_TIMEOUT},
                                               {UX_REENTRY, ERROR_RETRY},
                                               {UX_INVALID_STATE, ERROR_INVALID_STATE},
                                               {UX_INVALID_PARAMETER, ERROR_INVALID_PARAMETER},
                                               {UX_ABORTED, ERROR_OPERATION_ABORTED},
                                               {UX_MATH_OVERFLOW, ERROR_ARITHMETIC_OVERFLOW},
                                               {UX_TOO_MANY_DEVICES, ERROR_NOT_ENOUGH_MEMORY},
                                               {UX_MEMORY_INSUFFICIENT, ERROR_NOT_ENOUGH_MEMORY},
                                               {UX_NO_TD_AVAILABLE, ERROR_NOT_ENOUGH_MEMORY},
                                               {UX_NO_ED_AVAILABLE, ERROR_NOT_ENOUGH_MEMORY},
                                               {UX_MEMORY_ARRAY_FULL, ERROR_NOT_ENOUGH_MEMORY},
                                               {UX_ALREADY_ACTIVATED, ERROR_ALREADY_EXISTS},
                                               {UX_TRANSFER_STALLED, ERROR_IO_INCOMPLETE},
                                               {UX_TRANSFER_NO_ANSWER, ERROR_DEVICE_NOT_CONNECTED},
                                               {UX_TRANSFER_ERROR, ERROR_IO_DEVICE},
                                               {UX_TRANSFER_NOT_READY, ERROR_NOT_READY},
                                               {UX_TRANSFER_BUFFER_OVERFLOW, ERROR_BUFFER_OVERFLOW},
                                               {UX_TRANSFER_DATA_LESS_THAN_EXPECTED, ERROR_MORE_DATA},
                                               {UX_TRANSFER_STATUS_ABORT, ERROR_OPERATION_ABORTED},
                                               {UX_CONTROLLER_DEAD, ERROR_DEVICE_NOT_CONNECTED},
                                               {UX_NO_BANDWIDTH_AVAILABLE, ERROR_NOT_ENOUGH_MEMORY},
                                               {UX_DESCRIPTOR_CORRUPTED, ERROR_IO_DEVICE},
                                               {UX_DEVICE_ENUMERATION_FAILURE, ERROR_IO_DEVICE},
                                               {UX_TOO_MANY_HUB_PORTS, ERROR_NOT_ENOUGH_MEMORY},
                                               {UX_DEVICE_HANDLE_UNKNOWN, ERROR_NOT_FOUND},
                                               {UX_CONFIGURATION_HANDLE_UNKNOWN, ERROR_NOT_FOUND},
                                               {UX_INTERFACE_HANDLE_UNKNOWN, ERROR_NOT_FOUND},
                                               {UX_ENDPOINT_HANDLE_UNKNOWN, ERROR_NOT_FOUND},
                                               {UX_FUNCTION_NOT_SUPPORTED, ERROR_CALL_NOT_IMPLEMENTED},
                                               {UX_CONTROLLER_UNKNOWN, ERROR_NOT_FOUND},
                                               {UX_PORT_INDEX_UNKNOWN, ERROR_INVALID_INDEX},
                                               {UX_NO_CLASS_MATCH, ERROR_CLASS_DOES_NOT_EXIST},
                                               {UX_HOST_CLASS_ALREADY_INSTALLED, ERROR_ALREADY_EXISTS},
                                               {UX_HOST_CLASS_UNKNOWN, ERROR_CLASS_DOES_NOT_EXIST},
                                               {UX_CONNECTION_INCOMPATIBLE, ERROR_INVALID_DATA},
                                               {UX_HOST_CLASS_INSTANCE_UNKNOWN, ERROR_NOT_FOUND},
                                               {UX_TRANSFER_TIMEOUT, ERROR_TIMEOUT},
                                               {UX_BUFFER_OVERFLOW, ERROR_BUFFER_OVERFLOW},
                                               {UX_NO_DEVICE_CONNECTED, ERROR_DEV_NOT_EXIST}};

static DWORD ux_status_to_win32 (UINT status)
{
    for (size_t i = 0; i < sizeof(error_map) / sizeof(UX_WIN32_ERROR_MAP); i++) {
        if (error_map[i].usbx_error == status) {
            return error_map[i].win32_error;
        }
    }
    return ERROR_GEN_FAILURE;
}
