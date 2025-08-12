#ifndef __NXDK_USB_H__
#define __NXDK_USB_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "ux_api.h"

#ifndef NX_USB_MEMORY_POOL_SIZE
#define NX_USB_MEMORY_POOL_SIZE (0x20000)
#endif

#ifndef NX_USB_DMA_MEMORY_POOL_SIZE
#define NX_USB_DMA_MEMORY_POOL_SIZE (0x20000)
#endif

// https://xboxdevwiki.net/USB
// Channel 1 is the retail OHCI controller
// Channel 9 is the second OHCI controller on debug kits.
#ifndef NX_USB_OHCI_IRQ_CHANNEL
#define NX_USB_OHCI_IRQ_CHANNEL (1)
#endif

// https://xboxdevwiki.net/Memory
// 0xFED00000 is the retail OHCI controller MMIO base.
// 0xFED08000 is the second OHCI controller on debug kits.
#ifndef NX_USB_OHCI_MMIO_BASE
#define NX_USB_OHCI_MMIO_BASE (0xFED00000)
#endif

typedef struct nx_usb_device {
    UX_DEVICE *ux_device;
    UX_DEVICE_DESCRIPTOR device_descriptor;
    UX_CONFIGURATION_DESCRIPTOR *configuration_descriptor;
    CRITICAL_SECTION lock;
} nx_usb_device_t;

typedef struct match_device_id {
    USHORT pid;
    USHORT vid;
} match_device_id_t;

typedef struct match_device_class {
    UCHAR bDeviceClass;
    UCHAR bDeviceSubClass;
    UCHAR bDeviceProtocol;
} match_device_class_t;

typedef struct match_interface_class {
    UCHAR bInterfaceClass;
    UCHAR bInterfaceSubClass;
    UCHAR bInterfaceProtocol;
} match_interface_class_t;

int nxUsbInit(void);
int nxUsbShutdown(void);
int nxUsbRegisterChangeCallback(void (*ux_system_host_change_function)(ULONG, UX_HOST_CLASS *, VOID *));
int nxUsbDeviceClaim(match_device_id_t *device_ids, match_device_class_t *device_class,
                     match_interface_class_t *interface_class, nx_usb_device_t *nx_device);
int nxUsbDeviceRelease(nx_usb_device_t *nx_device);
void nxUsbLock(nx_usb_device_t *nx_device);
void nxUsbUnlock(nx_usb_device_t *nx_device);
#ifdef __cplusplus
}
#endif

#endif //__NXDK_USB_H__
