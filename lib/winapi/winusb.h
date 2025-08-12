// SPDX-License-Identifier: CC0-1.0

#ifndef __WUSB_H__
#define __WUSB_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <usb100.h>
#include <windows.h>
#include <winusbio.h>

typedef PVOID WINUSB_INTERFACE_HANDLE, *PWINUSB_INTERFACE_HANDLE;
typedef PVOID WINUSB_ISOCH_BUFFER_HANDLE, *PWINUSB_ISOCH_BUFFER_HANDLE;
typedef PVOID USB_FRAME_NUMBER_AND_QPC_FOR_TIME_SYNC_INFORMATION, *PUSB_FRAME_NUMBER_AND_QPC_FOR_TIME_SYNC_INFORMATION;

#pragma pack(push, 1)

typedef union _REQUEST_TYPE
{
    struct
    {
        UCHAR Recipient : 2;
        UCHAR Reserved : 3;
        UCHAR Type : 2;
        UCHAR Dir : 1;
    } Bits;
    UCHAR Byte;
} REQUEST_TYPE, *PREQUEST_TYPE;

typedef struct _WINUSB_SETUP_PACKET
{
    UCHAR RequestType;
    UCHAR Request;
    USHORT Value;
    USHORT Index;
    USHORT Length;
} WINUSB_SETUP_PACKET, *PWINUSB_SETUP_PACKET;
#pragma pack(pop)

typedef LONG USBD_STATUS;

typedef struct _USBD_ISO_PACKET_DESCRIPTOR
{
    ULONG Offset;
    ULONG Length;
    USBD_STATUS Status;
} USBD_ISO_PACKET_DESCRIPTOR, *PUSBD_ISO_PACKET_DESCRIPTOR;

BOOL WinUsb_AbortPipe (WINUSB_INTERFACE_HANDLE InterfaceHandle, UCHAR PipeID);
BOOL WinUsb_ControlTransfer (WINUSB_INTERFACE_HANDLE InterfaceHandle, WINUSB_SETUP_PACKET SetupPacket, PUCHAR Buffer,
                             ULONG BufferLength, PULONG LengthTransferred, LPOVERLAPPED Overlapped);
BOOL WinUsb_FlushPipe (WINUSB_INTERFACE_HANDLE InterfaceHandle, UCHAR PipeID);
BOOL WinUsb_Free (WINUSB_INTERFACE_HANDLE InterfaceHandle);
BOOL WinUsb_GetAdjustedFrameNumber (PULONG CurrentFrameNumber, LARGE_INTEGER TimeStamp);
BOOL WinUsb_GetCurrentFrameNumberAndQpc (WINUSB_INTERFACE_HANDLE InterfaceHandle,
                                         PUSB_FRAME_NUMBER_AND_QPC_FOR_TIME_SYNC_INFORMATION FrameQpcInfo);
BOOL WinUsb_GetAssociatedInterface (WINUSB_INTERFACE_HANDLE InterfaceHandle, UCHAR AssociatedInterfaceIndex,
                                    PWINUSB_INTERFACE_HANDLE AssociatedInterfaceHandle);
BOOL WinUsb_GetCurrentAlternateSetting (WINUSB_INTERFACE_HANDLE InterfaceHandle, PUCHAR SettingNumber);
BOOL WinUsb_GetCurrentFrameNumber (WINUSB_INTERFACE_HANDLE InterfaceHandle, PULONG CurrentFrameNumber,
                                   LARGE_INTEGER *TimeStamp);
BOOL WinUsb_GetDescriptor (WINUSB_INTERFACE_HANDLE InterfaceHandle, UCHAR DescriptorType, UCHAR Index,
                           USHORT LanguageID, PUCHAR Buffer, ULONG BufferLength, PULONG LengthTransferred);
BOOL WinUsb_GetOverlappedResult (WINUSB_INTERFACE_HANDLE InterfaceHandle, LPOVERLAPPED lpOverlapped,
                                 LPDWORD lpNumberOfBytesTransferred, BOOL bWait);
BOOL WinUsb_GetPipePolicy (WINUSB_INTERFACE_HANDLE InterfaceHandle, UCHAR PipeID, ULONG PolicyType, PULONG ValueLength,
                           PVOID Value);
BOOL WinUsb_GetPowerPolicy (WINUSB_INTERFACE_HANDLE InterfaceHandle, ULONG PolicyType, PULONG ValueLength, PVOID Value);
BOOL WinUsb_Initialize (HANDLE DeviceHandle, PWINUSB_INTERFACE_HANDLE InterfaceHandle);
PUSB_INTERFACE_DESCRIPTOR WinUsb_ParseConfigurationDescriptor (PUSB_CONFIGURATION_DESCRIPTOR ConfigurationDescriptor,
                                                               PVOID StartPosition, LONG InterfaceNumber,
                                                               LONG AlternateSetting, LONG InterfaceClass,
                                                               LONG InterfaceSubClass, LONG InterfaceProtocol);
PUSB_COMMON_DESCRIPTOR WinUsb_ParseDescriptors (PVOID DescriptorBuffer, ULONG TotalLength, PVOID StartPosition,
                                                LONG DescriptorType);
BOOL WinUsb_ReadIsochPipe (WINUSB_ISOCH_BUFFER_HANDLE BufferHandle, ULONG Offset, ULONG Length, PULONG FrameNumber,
                           ULONG NumberOfPackets, PUSBD_ISO_PACKET_DESCRIPTOR IsoPacketDescriptors,
                           LPOVERLAPPED Overlapped);
BOOL WinUsb_ReadIsochPipeAsap (WINUSB_ISOCH_BUFFER_HANDLE BufferHandle, ULONG Offset, ULONG Length, BOOL ContinueStream,
                               ULONG NumberOfPackets, PUSBD_ISO_PACKET_DESCRIPTOR IsoPacketDescriptors,
                               LPOVERLAPPED Overlapped);
BOOL WinUsb_ReadPipe (WINUSB_INTERFACE_HANDLE InterfaceHandle, UCHAR PipeID, PUCHAR Buffer, ULONG BufferLength,
                      PULONG LengthTransferred, LPOVERLAPPED Overlapped);
BOOL WinUsb_RegisterIsochBuffer (WINUSB_INTERFACE_HANDLE InterfaceHandle, UCHAR PipeID, PUCHAR Buffer,
                                 ULONG BufferLength, PWINUSB_ISOCH_BUFFER_HANDLE IsochBufferHandle);
BOOL WinUsb_QueryDeviceInformation (WINUSB_INTERFACE_HANDLE InterfaceHandle, ULONG InformationType, PULONG BufferLength,
                                    PVOID Buffer);
BOOL WinUsb_QueryInterfaceSettings (WINUSB_INTERFACE_HANDLE InterfaceHandle, UCHAR AlternateInterfaceNumber,
                                    PUSB_INTERFACE_DESCRIPTOR UsbAltInterfaceDescriptor);
BOOL WinUsb_QueryPipe (WINUSB_INTERFACE_HANDLE InterfaceHandle, UCHAR AlternateInterfaceNumber, UCHAR PipeIndex,
                       PWINUSB_PIPE_INFORMATION PipeInformation);
BOOL WinUsb_QueryPipeEx (WINUSB_INTERFACE_HANDLE InterfaceHandle, UCHAR AlternateInterfaceNumber, UCHAR PipeIndex,
                         PWINUSB_PIPE_INFORMATION_EX PipeInformationEx);
BOOL WinUsb_ResetPipe (WINUSB_INTERFACE_HANDLE InterfaceHandle, UCHAR PipeID);
BOOL WinUsb_SetCurrentAlternateSetting (WINUSB_INTERFACE_HANDLE InterfaceHandle, UCHAR SettingNumber);
BOOL WinUsb_SetPipePolicy (WINUSB_INTERFACE_HANDLE InterfaceHandle, UCHAR PipeID, ULONG PolicyType, ULONG ValueLength,
                           PVOID Value);
BOOL WinUsb_SetPowerPolicy (WINUSB_INTERFACE_HANDLE InterfaceHandle, ULONG PolicyType, ULONG ValueLength, PVOID Value);
BOOL WinUsb_UnregisterIsochBuffer (WINUSB_ISOCH_BUFFER_HANDLE IsochBufferHandle);
BOOL WinUsb_WriteIsochPipe (WINUSB_ISOCH_BUFFER_HANDLE BufferHandle, ULONG Offset, ULONG Length, PULONG FrameNumber,
                            LPOVERLAPPED Overlapped);
BOOL WinUsb_WriteIsochPipeAsap (WINUSB_ISOCH_BUFFER_HANDLE BufferHandle, ULONG Offset, ULONG Length,
                                BOOL ContinueStream, LPOVERLAPPED Overlapped);
BOOL WinUsb_WritePipe (WINUSB_INTERFACE_HANDLE InterfaceHandle, UCHAR PipeID, PUCHAR Buffer, ULONG BufferLength,
                       PULONG LengthTransferred, LPOVERLAPPED Overlapped);

// Source https://github.com/reactos/reactos/blob/master/sdk/include/psdk/usb.h
// SPDX-License-Identifier: CC0-1.0
// clang-format off
#define USBD_SUCCESS(Status)                            ((USBD_STATUS)(Status) >= 0)
#define USBD_PENDING(Status)                            ((ULONG)(Status) >> 30 == 1)
#define USBD_ERROR(Status)                              ((USBD_STATUS)(Status) < 0)
#define USBD_STATUS_SUCCESS                             ((USBD_STATUS)0x00000000L)
#define USBD_STATUS_PENDING                             ((USBD_STATUS)0x40000000L)
#define USBD_STATUS_CRC                                 ((USBD_STATUS)0xC0000001L)
#define USBD_STATUS_BTSTUFF                             ((USBD_STATUS)0xC0000002L)
#define USBD_STATUS_DATA_TOGGLE_MISMATCH                ((USBD_STATUS)0xC0000003L)
#define USBD_STATUS_STALL_PID                           ((USBD_STATUS)0xC0000004L)
#define USBD_STATUS_DEV_NOT_RESPONDING                  ((USBD_STATUS)0xC0000005L)
#define USBD_STATUS_PID_CHECK_FAILURE                   ((USBD_STATUS)0xC0000006L)
#define USBD_STATUS_UNEXPECTED_PID                      ((USBD_STATUS)0xC0000007L)
#define USBD_STATUS_DATA_OVERRUN                        ((USBD_STATUS)0xC0000008L)
#define USBD_STATUS_DATA_UNDERRUN                       ((USBD_STATUS)0xC0000009L)
#define USBD_STATUS_RESERVED1                           ((USBD_STATUS)0xC000000AL)
#define USBD_STATUS_RESERVED2                           ((USBD_STATUS)0xC000000BL)
#define USBD_STATUS_BUFFER_OVERRUN                      ((USBD_STATUS)0xC000000CL)
#define USBD_STATUS_BUFFER_UNDERRUN                     ((USBD_STATUS)0xC000000DL)
#define USBD_STATUS_NOT_ACCESSED                        ((USBD_STATUS)0xC000000FL)
#define USBD_STATUS_FIFO                                ((USBD_STATUS)0xC0000010L)
#define USBD_STATUS_XACT_ERROR                          ((USBD_STATUS)0xC0000011L)
#define USBD_STATUS_BABBLE_DETECTED                     ((USBD_STATUS)0xC0000012L)
#define USBD_STATUS_DATA_BUFFER_ERROR                   ((USBD_STATUS)0xC0000013L)
#define USBD_STATUS_ENDPOINT_HALTED                     ((USBD_STATUS)0xC0000030L)
#define USBD_STATUS_INVALID_URB_FUNCTION                ((USBD_STATUS)0x80000200L)
#define USBD_STATUS_INVALID_PARAMETER                   ((USBD_STATUS)0x80000300L)
#define USBD_STATUS_ERROR_BUSY                          ((USBD_STATUS)0x80000400L)
#define USBD_STATUS_INVALID_PIPE_HANDLE                 ((USBD_STATUS)0x80000600L)
#define USBD_STATUS_NO_BANDWIDTH                        ((USBD_STATUS)0x80000700L)
#define USBD_STATUS_INTERNAL_HC_ERROR                   ((USBD_STATUS)0x80000800L)
#define USBD_STATUS_ERROR_SHORT_TRANSFER                ((USBD_STATUS)0x80000900L)
#define USBD_STATUS_BAD_START_FRAME                     ((USBD_STATUS)0xC0000A00L)
#define USBD_STATUS_ISOCH_REQUEST_FAILED                ((USBD_STATUS)0xC0000B00L)
#define USBD_STATUS_FRAME_CONTROL_OWNED                 ((USBD_STATUS)0xC0000C00L)
#define USBD_STATUS_FRAME_CONTROL_NOT_OWNED             ((USBD_STATUS)0xC0000D00L)
#define USBD_STATUS_NOT_SUPPORTED                       ((USBD_STATUS)0xC0000E00L)
#define USBD_STATUS_INVALID_CONFIGURATION_DESCRIPTOR    ((USBD_STATUS)0xC0000F00L)
#define USBD_STATUS_INSUFFICIENT_RESOURCES              ((USBD_STATUS)0xC0001000L)
#define USBD_STATUS_SET_CONFIG_FAILED                   ((USBD_STATUS)0xC0002000L)
#define USBD_STATUS_BUFFER_TOO_SMALL                    ((USBD_STATUS)0xC0003000L)
#define USBD_STATUS_INTERFACE_NOT_FOUND                 ((USBD_STATUS)0xC0004000L)
#define USBD_STATUS_INVALID_PIPE_FLAGS                  ((USBD_STATUS)0xC0005000L)
#define USBD_STATUS_TIMEOUT                             ((USBD_STATUS)0xC0006000L)
#define USBD_STATUS_DEVICE_GONE                         ((USBD_STATUS)0xC0007000L)
#define USBD_STATUS_STATUS_NOT_MAPPED                   ((USBD_STATUS)0xC0008000L)
#define USBD_STATUS_HUB_INTERNAL_ERROR                  ((USBD_STATUS)0xC0009000L)
#define USBD_STATUS_CANCELED                            ((USBD_STATUS)0xC0010000L)
#define USBD_STATUS_ISO_NOT_ACCESSED_BY_HW              ((USBD_STATUS)0xC0020000L)
#define USBD_STATUS_ISO_TD_ERROR                        ((USBD_STATUS)0xC0030000L)
#define USBD_STATUS_ISO_NA_LATE_USBPORT                 ((USBD_STATUS)0xC0040000L)
#define USBD_STATUS_ISO_NOT_ACCESSED_LATE               ((USBD_STATUS)0xC0050000L)
#define USBD_STATUS_BAD_DESCRIPTOR                      ((USBD_STATUS)0xC0100000L)
#define USBD_STATUS_BAD_DESCRIPTOR_BLEN                 ((USBD_STATUS)0xC0100001L)
#define USBD_STATUS_BAD_DESCRIPTOR_TYPE                 ((USBD_STATUS)0xC0100002L)
#define USBD_STATUS_BAD_INTERFACE_DESCRIPTOR            ((USBD_STATUS)0xC0100003L)
#define USBD_STATUS_BAD_ENDPOINT_DESCRIPTOR             ((USBD_STATUS)0xC0100004L)
#define USBD_STATUS_BAD_INTERFACE_ASSOC_DESCRIPTOR      ((USBD_STATUS)0xC0100005L)
#define USBD_STATUS_BAD_CONFIG_DESC_LENGTH              ((USBD_STATUS)0xC0100006L)
#define USBD_STATUS_BAD_NUMBER_OF_INTERFACES            ((USBD_STATUS)0xC0100007L)
#define USBD_STATUS_BAD_NUMBER_OF_ENDPOINTS             ((USBD_STATUS)0xC0100008L)
#define USBD_STATUS_BAD_ENDPOINT_ADDRESS                ((USBD_STATUS)0xC0100009L)
//clang-format on

#ifdef __cplusplus
}
#endif

#endif
