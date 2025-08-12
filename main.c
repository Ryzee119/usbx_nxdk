#include <assert.h>
#include <hal/debug.h>
#include <hal/video.h>
#include <nxdk/usb.h>
#include <windows.h>
#include <winusb.h>

#undef printf
#define printf DbgPrint

DWORD WINAPI ohci_print (void *param);

const char *get_error_string (int error_code);

static void host_change_function (ULONG change_code, UX_HOST_CLASS *ux_class, VOID *param)
{
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

int main (void)
{
    XVideoSetMode(640, 480, 16, REFRESH_DEFAULT);

    debugPrint("%s\n", _ux_version_id);
    nxUsbInit();
    nxUsbRegisterChangeCallback(host_change_function);

    CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) ohci_print, NULL, 0, NULL);
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
ULONG _tx_time_get (VOID)
{
    return GetTickCount();
}

UINT _tx_thread_interrupt_disable (void)
{
    return KeRaiseIrqlToDpcLevel();
}

void _tx_thread_interrupt_restore (UINT old_posture)
{
    KfLowerIrql(old_posture);
}
#endif

typedef volatile struct
{
    uint32_t revision;

    union
    {
        uint32_t control;
        struct
        {
            uint32_t control_bulk_service_ratio : 2;
            uint32_t periodic_list_enable : 1;
            uint32_t isochronous_enable : 1;
            uint32_t control_list_enable : 1;
            uint32_t bulk_list_enable : 1;
            uint32_t hc_functional_state : 2;
            uint32_t interrupt_routing : 1;
            uint32_t remote_wakeup_connected : 1;
            uint32_t remote_wakeup_enale : 1;
            uint32_t TU_RESERVED : 21;
        } control_bit;
    };

    union
    {
        uint32_t command_status;
        struct
        {
            uint32_t controller_reset : 1;
            uint32_t control_list_filled : 1;
            uint32_t bulk_list_filled : 1;
            uint32_t ownership_change_request : 1;
            uint32_t : 12;
            uint32_t scheduling_overrun_count : 2;
        } command_status_bit;
    };

    uint32_t interrupt_status;
    uint32_t interrupt_enable;
    uint32_t interrupt_disable;

    uint32_t hcca;
    uint32_t period_current_ed;
    uint32_t control_head_ed;
    uint32_t control_current_ed;
    uint32_t bulk_head_ed;
    uint32_t bulk_current_ed;
    uint32_t done_head;

    uint32_t frame_interval;
    uint32_t frame_remaining;
    uint32_t frame_number;
    uint32_t periodic_start;
    uint32_t lowspeed_threshold;

    union
    {
        uint32_t rh_descriptorA;
        struct
        {
            uint32_t number_downstream_ports : 8;
            uint32_t power_switching_mode : 1;
            uint32_t no_power_switching : 1;
            uint32_t device_type : 1;
            uint32_t overcurrent_protection_mode : 1;
            uint32_t no_over_current_protection : 1;
            uint32_t reserved : 11;
            uint32_t power_on_to_good_time : 8;
        } rh_descriptorA_bit;
    };

    union
    {
        uint32_t rh_descriptorB;
        struct
        {
            uint32_t device_removable : 16;
            uint32_t port_power_control_mask : 16;
        } rh_descriptorB_bit;
    };

    union
    {
        uint32_t rh_status;
        struct
        {
            uint32_t local_power_status : 1; // read Local Power Status; write: Clear Global Power
            uint32_t over_current_indicator : 1;
            uint32_t : 13;
            uint32_t device_remote_wakeup_enable : 1;
            uint32_t local_power_status_change : 1;
            uint32_t over_current_indicator_change : 1;
            uint32_t : 13;
            uint32_t clear_remote_wakeup_enable : 1;
        } rh_status_bit;
    };

    union
    {
        uint32_t rhport_status[4];
        struct
        {
            uint32_t current_connect_status : 1;
            uint32_t port_enable_status : 1;
            uint32_t port_suspend_status : 1;
            uint32_t port_over_current_indicator : 1;
            uint32_t port_reset_status : 1;
            uint32_t : 3;
            uint32_t port_power_status : 1;
            uint32_t low_speed_device_attached : 1;
            uint32_t : 6;
            uint32_t connect_status_change : 1;
            uint32_t port_enable_status_change : 1;
            uint32_t port_suspend_status_change : 1;
            uint32_t port_over_current_indicator_change : 1;
            uint32_t port_reset_status_change : 1;
            uint32_t TU_RESERVED : 11;
        } rhport_status_bit[4];
    };
} ohci_registers_t;

#include <hal/debug.h>
#include <windows.h>
// thread to print ohci register values
#define OHCI_REG ((volatile ohci_registers_t *)(0xFED00000))
#include "ux_hcd_ohci.h"
VOID *_ux_utility_virtual_address (VOID *physical_address);
DWORD WINAPI ohci_print (void *param)
{
    UX_HCD_OHCI_HCCA *hcca = (UX_HCD_OHCI_HCCA *)_ux_utility_virtual_address((VOID *)OHCI_REG->hcca);
    while (1) {
        debugPrint("HcRevision: %x\n", OHCI_REG->revision);
        debugPrint("HcControl: %x\n", OHCI_REG->control);
        debugPrint("HcCommandStatus: %x\n", OHCI_REG->command_status);
        debugPrint("HcInterruptStatus: %x\n", OHCI_REG->interrupt_status);
        debugPrint("HcInterruptEnable: %x\n", OHCI_REG->interrupt_enable);
        debugPrint("HcInterruptDisable: %x\n", OHCI_REG->interrupt_disable);
        debugPrint("HcHCCA: %x donehead %x\n", OHCI_REG->hcca, hcca->ux_hcd_ohci_hcca_done_head);
        debugPrint("HcPeriodCurrentED: %x\n", OHCI_REG->period_current_ed);
        debugPrint("HcControlHeadED: %x\n", OHCI_REG->control_head_ed);
        debugPrint("HcControlCurrentED: %x\n", OHCI_REG->control_current_ed);
        debugPrint("HcBulkHeadED: %x\n", OHCI_REG->bulk_head_ed);
        debugPrint("HcBulkCurrentED: %x\n", OHCI_REG->bulk_current_ed);
        debugPrint("HcDoneHead: %x\n", OHCI_REG->done_head);
        debugPrint("HcFmInterval: %x\n", OHCI_REG->frame_interval);
        debugPrint("HcFmRemaining: %x\n", OHCI_REG->frame_remaining);
        debugPrint("HcFmNumber: %x\n", OHCI_REG->frame_number);
        debugPrint("HcPeriodicStart: %x\n", OHCI_REG->periodic_start);
        debugPrint("HcLSThreshold: %x\n", OHCI_REG->lowspeed_threshold);
        debugPrint("HcRhDescriptorA: %x\n", OHCI_REG->rh_descriptorA);
        debugPrint("HcRhDescriptorB: %x\n", OHCI_REG->rh_descriptorB);
        debugPrint("HcRhStatus: %x\n", OHCI_REG->rh_status);

        debugResetCursor();
        Sleep(1);
    }
    // debugPrint("HcRhPortStatus1: %x\n", OHCI_REG->rhport_status[0]);
    // debugPrint("HcRhPortStatus2: %x\n", OHCI_REG->rhport_status[1]);
    // debugPrint("HcRhPortStatus3: %x\n", OHCI_REG->rhport_status[2]);
    // debugPrint("HcRhPortStatus3: %x\n", OHCI_REG->rhport_status[2]);
}