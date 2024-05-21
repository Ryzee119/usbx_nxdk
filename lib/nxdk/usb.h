#ifndef __NXDK_USB_H__
#define __NXDK_USB_H__

#ifdef __cplusplus
extern "C" {
#endif

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
#    define NX_USB_OHCI_MMIO_BASE (0xFED00000)
#endif

int nxUsbInit(void);
int nxUsbShutdown(void);
int nxUsbRegisterEventCallback(UINT (*ux_system_host_change_function)(ULONG, UX_HOST_CLASS *, VOID *));

#ifdef __cplusplus
}
#endif

#endif //__NXDK_USB_H__
