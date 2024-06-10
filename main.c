#include <hal/debug.h>
#include <hal/video.h>
#include <nxdk/usb.h>
#include <windows.h>
#include <assert.h>
#include <winusb.h>

#undef printf
#define printf DbgPrint

const char *get_error_string(int error_code);

static void host_change_function(ULONG change_code, UX_HOST_CLASS *ux_class, VOID *param) {
    switch (change_code) {
        case UX_DEVICE_INSERTION:
            // printf("USB device inserted\r\n");
            break;
        case UX_DEVICE_REMOVAL:
        case UX_DEVICE_DISCONNECTION:
            debugPrint("USB deviffce removed\r\n");
            break;
        case UX_DEVICE_CONNECTION:
            UX_DEVICE *dev = param;
            if (dev->ux_device_state == UX_DEVICE_CONFIGURED || dev->ux_device_state == UX_DEVICE_ADDRESSED) {
                debugPrint("USB device connected 0x%04x 0x%04x\r\n", dev->ux_device_descriptor.idVendor,
                           dev->ux_device_descriptor.idProduct);
            } else {
                debugPrint("Failed enumeration %02lx\r\n", dev->ux_device_state);
            }
            break;
        default:
            // printf("Unknown USB device change %02lx\n", change_code);
            break;
    }
    return;
}

int main(void) {
    XVideoSetMode(640, 480, 16, REFRESH_DEFAULT);

    debugPrint("%s\n", _ux_version_id);
    nxUsbInit();
    nxUsbRegisterChangeCallback(host_change_function);

    match_interface_class_t xid_interface = {0x58, 0x42, 0x00};

    nx_usb_device_t usb_device;
    WINUSB_INTERFACE_HANDLE winusbHandle = {0};
    BOOL claimed = FALSE;
    while (1) {
#ifdef UX_HOST_STANDALONE
        _ux_system_tasks_run();
#endif
        Sleep(1);

        if (nxUsbDeviceClaim(NULL, NULL, &xid_interface, &usb_device) >= 0) {
            printf("XID device claimed\r\n");

            if (WinUsb_Initialize(&usb_device, &winusbHandle) == TRUE) {
                printf("WinUSB initialized\r\n");
                claimed = TRUE;

                UCHAR buffer[64];

                // Frame timing
                ULONG frame_number;
                LARGE_INTEGER timestamp;
                if (WinUsb_GetCurrentFrameNumber(winusbHandle, &frame_number, &timestamp) == TRUE) {
                    printf("WinUSB frame number %lu\r\n", frame_number);
                    Sleep(100);
                    if (WinUsb_GetAdjustedFrameNumber(&frame_number, timestamp) == TRUE) {
                        printf("WinUSB adjusted frame number %lu\r\n", frame_number);
                    }
                }

                // Descriptotors
                UCHAR desc_buff[128];
                ULONG length_transferred;
                if (WinUsb_GetDescriptor(winusbHandle, USB_DEVICE_DESCRIPTOR_TYPE, 0, 0, desc_buff, sizeof(desc_buff),
                                         &length_transferred) == TRUE) {
                    printf("WinUSB device descriptor ");
                    for (ULONG i = 0; i < length_transferred; i++) {
                        printf("%02x ", desc_buff[i]);
                    }
                    printf("\r\n");
                }

                if (WinUsb_GetDescriptor(winusbHandle, USB_CONFIGURATION_DESCRIPTOR_TYPE, 0, 0, desc_buff,
                                         sizeof(UX_CONFIGURATION_DESCRIPTOR), &length_transferred) == TRUE) {
                    printf("WinUSB configuration descriptor ");
                    for (ULONG i = 0; i < length_transferred; i++) {
                        printf("%02x ", desc_buff[i]);
                    }
                    printf("\r\n");
                }

                if (WinUsb_GetDescriptor(winusbHandle, USB_CONFIGURATION_DESCRIPTOR_TYPE, 0, 0, desc_buff,
                                         sizeof(desc_buff), &length_transferred) == TRUE) {
                    printf("WinUSB configuration descriptor ");
                    for (ULONG i = 0; i < length_transferred; i++) {
                        printf("%02x ", desc_buff[i]);
                    }
                    printf("\r\n");
                } else {
                    printf("WinUSB configuration descriptor failed\r\n");
                }

                if (WinUsb_GetDescriptor(winusbHandle, USB_CONFIGURATION_DESCRIPTOR_TYPE, 0, 0, desc_buff,
                                         sizeof(desc_buff), &length_transferred) == TRUE) {
                    printf("WinUSB configuration descriptor ");
                    for (ULONG i = 0; i < length_transferred; i++) {
                        printf("%02x ", desc_buff[i]);
                    }
                    printf("\r\n");
                } else {
                    printf("WinUSB configuration descriptor failed\r\n");
                }

                if (WinUsb_QueryDeviceInformation(winusbHandle, DEVICE_SPEED, &length_transferred, buffer) == TRUE) {
                    printf("WinUSB device speed %02x\r\n", buffer[0]);
                }

                if (WinUsb_GetCurrentAlternateSetting(winusbHandle, &buffer[0]) == TRUE) {
                    printf("WinUSB current alternate setting %02x\r\n", buffer[0]);
                }

                if (WinUsb_SetCurrentAlternateSetting(winusbHandle, 1) == TRUE) {
                    printf("WinUSB set current alternate setting 0\r\n");
                } else {
                    printf("WinUSB set current alternate setting 0 failed %d: %s\r\n", GetLastError(),
                           get_error_string(GetLastError()));
                }

                WINUSB_INTERFACE_HANDLE AssociatedInterfaceHandle;
                if (WinUsb_GetAssociatedInterface(winusbHandle, 1, &AssociatedInterfaceHandle) == TRUE) {
                    printf("WinUSB associated interface handle %p\r\n", AssociatedInterfaceHandle);
                } else {
                    printf("WinUSB associated interface handle failed %d: %s\r\n", GetLastError(),
                           get_error_string(GetLastError()));
                }

                WINUSB_PIPE_INFORMATION pipe_info;
                if (WinUsb_QueryPipe(winusbHandle, 0, 0, &pipe_info)) {
                    printf("Pipe info: %d %02x %d %d\r\n", pipe_info.PipeType, pipe_info.PipeId,
                           pipe_info.MaximumPacketSize, pipe_info.Interval);
                } else {
                    printf("Failed to query pipe. %d\r\n", GetLastError());
                }

                WINUSB_PIPE_INFORMATION_EX pipe_infoex;
                if (WinUsb_QueryPipeEx(winusbHandle, 0, 0, &pipe_infoex)) {
                    printf("Pipe info: %d %02x %d %d %d\r\n", pipe_infoex.PipeType, pipe_infoex.PipeId,
                           pipe_infoex.MaximumPacketSize, pipe_infoex.Interval, pipe_infoex.MaximumBytesPerInterval);
                }

                // Read data from endpoint
                OVERLAPPED overlapped = {.Offset = 0, .OffsetHigh = 0, .hEvent = CreateEvent(NULL, FALSE, FALSE, NULL)};
                while (1) {
                    BOOL success = WinUsb_ReadPipe(winusbHandle, 0x82, buffer, 32, &length_transferred, &overlapped);
                    if (success == FALSE && GetLastError() != ERROR_IO_PENDING) {
                        printf("WinUSB read pipe failed %d: %s\r\n", GetLastError(), get_error_string(GetLastError()));
                        break;
                    } else {
                        success = WinUsb_GetOverlappedResult(winusbHandle, &overlapped, &length_transferred, TRUE);
                        if (success) {
                            for (ULONG i = 0; i < length_transferred; i++) {
                                printf("%02x ", buffer[i]);
                            }
                            printf("\r\n");
                        } else {
                            printf("WinUSB read pipe failed %d: %s\r\n", GetLastError(),
                                   get_error_string(GetLastError()));
                            break;
                        }
                    }
                }

            } else {
                printf("WinUSB initialization failed\r\n");
            }
        }

        uint8_t control_buffer[32];
        ULONG length_transferred;
        const REQUEST_TYPE RequestType = {
            .Bits.Recipient = BMREQUEST_TO_INTERFACE,
            .Bits.Reserved = 0,
            .Bits.Type = BMREQUEST_CLASS,
            .Bits.Dir = BMREQUEST_DEVICE_TO_HOST,
        };
        WINUSB_SETUP_PACKET SetupPacket = {
            .RequestType = RequestType.Byte,
            .Request = 1,
            .Value = 0x0100,
            .Index = 0,
            .Length = 0x14,
        };
        if (0 && claimed) {
            memset(control_buffer, 0, sizeof(control_buffer));
            if (WinUsb_ControlTransfer(winusbHandle, SetupPacket, control_buffer, sizeof(control_buffer),
                                       &length_transferred, NULL) == TRUE) {
                printf("WinUSB control transfer ");
                for (ULONG i = 0; i < length_transferred; i++) {
                    printf("%02x ", control_buffer[i]);
                }
                printf(": %p\r\n", control_buffer);
            } else {
                printf("WinUSB control transfer failed %d: %s\r\n", GetLastError(), get_error_string(GetLastError()));
            }
        }
    }

    nxUsbShutdown();
    return 0;
}

#ifdef UX_HOST_STANDALONE
ULONG _tx_time_get(VOID) { return GetTickCount(); }

UINT _tx_thread_interrupt_disable(void) { return KeRaiseIrqlToDpcLevel(); }

void _tx_thread_interrupt_restore(UINT old_posture) { KfLowerIrql(old_posture); }
#endif
