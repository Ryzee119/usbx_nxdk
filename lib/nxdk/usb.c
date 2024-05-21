// SPDX-License-Identifier: MIT

// SPDX-FileCopyrightText: 2024 Ryan Wendland

#include <stdlib.h>
#include <assert.h>
#include <hal/debug.h>
#include <hal/video.h>
#include <windows.h>

#include "ux_api.h"
#include "ux_system.h"
#include "ux_utility.h"
#include "ux_host_class_hub.h"
#include "ux_hcd_ohci.h"

#include "usb.h"

typedef struct nx_usb_change_callback {
    UINT (*ux_system_host_change_function)(ULONG, UX_HOST_CLASS *, VOID *);
    LIST_ENTRY entry;
} nx_usb_change_callback_t;

typedef struct nx_usb_data {
    UX_HCD_OHCI *hcd_ohci;
    KINTERRUPT irq;
    KDPC dpc;
    KEVENT evt;
    HANDLE irq_thread;
    HANDLE task_thread;
    uint32_t count;
    uint8_t *regular_memory_pool;
    uint8_t *dma_memory_pool;
    LIST_ENTRY change_callback_head;
} nx_usb_data_t;
static nx_usb_data_t usb = {0};

static BOOLEAN NTAPI isr(PKINTERRUPT Interrupt, PVOID ServiceContext) {
    nx_usb_data_t *nx_usb_data = &usb;
    _ux_hcd_ohci_register_write(usb.hcd_ohci, OHCI_HC_INTERRUPT_DISABLE, OHCI_HC_INT_MIE);
    KeInsertQueueDpc(&nx_usb_data->dpc, NULL, NULL);
    return TRUE;
}

static void NTAPI dpc(PKDPC Dpc, PVOID DeferredContext, PVOID arg1, PVOID arg2) {
    nx_usb_data_t *nx_usb_data = &usb;
    KeSetEvent(&nx_usb_data->evt, IO_KEYBOARD_INCREMENT, FALSE);
    return;
}

static DWORD WINAPI irq_process(LPVOID lpThreadParameter) {
    nx_usb_data_t *nx_usb_data = lpThreadParameter;
    while (1) {
        KeWaitForSingleObject(&nx_usb_data->evt, Executive, KernelMode, FALSE, NULL);
        if (nx_usb_data->count == 0) {
            break;
        }
        ux_hcd_ohci_interrupt_handler();
        _ux_hcd_ohci_register_write(usb.hcd_ohci, OHCI_HC_INTERRUPT_ENABLE, OHCI_HC_INT_MIE);
    }
    return 0;
}

static UINT host_change_function(ULONG change_code, UX_HOST_CLASS *ux_class, VOID *param) {
    PLIST_ENTRY entry = usb.change_callback_head.Flink;
    while (entry != &usb.change_callback_head) {
        nx_usb_change_callback_t *callback = CONTAINING_RECORD(entry, nx_usb_change_callback_t, entry);
        callback->ux_system_host_change_function(change_code, ux_class, param);
        entry = entry->Flink;
    }

#if (1)
    switch (change_code) {
        case UX_DEVICE_INSERTION:
            debugPrint("USB device inserted\n");
            break;
        case UX_DEVICE_REMOVAL:
        case UX_DEVICE_DISCONNECTION:
            debugPrint("USB device removed\n");
            break;
        case UX_DEVICE_CONNECTION:
            UX_DEVICE *dev = param;
            debugPrint("USB device connected 0x%04x 0x%04x\n", dev->ux_device_descriptor.idVendor,
                       dev->ux_device_descriptor.idProduct);
            break;
        default:
            debugPrint("Unknown USB device change %02lx\n", change_code);
            break;
    }
#endif
    return UX_SUCCESS;
}

int nxUsbInit() {
    if (++usb.count > 1) {
        return usb.count;
    }

    debugPrint("%s\n", _ux_version_id);

    usb.change_callback_head.Flink = &usb.change_callback_head;
    usb.change_callback_head.Blink = &usb.change_callback_head;

    KIRQL irql;
    ULONG vector = HalGetInterruptVector((ULONG)NX_USB_OHCI_IRQ_CHANNEL, &irql);
    KeInitializeInterrupt(&usb.irq, &isr, &usb, vector, irql, LevelSensitive, FALSE);
    KeInitializeDpc(&usb.dpc, dpc, &usb);
    KeInitializeEvent(&usb.evt, SynchronizationEvent, FALSE);
    KeConnectInterrupt(&usb.irq);

    usb.irq_thread = CreateThread(NULL, 0, irq_process, &usb, 0, NULL);

    usb.regular_memory_pool = malloc(NX_USB_MEMORY_POOL_SIZE);
    usb.dma_memory_pool = MmAllocateContiguousMemory(NX_USB_DMA_MEMORY_POOL_SIZE);
    if (!usb.regular_memory_pool || !usb.dma_memory_pool) {
        goto init_failure;
    }

    if (ux_system_initialize(usb.regular_memory_pool, NX_USB_MEMORY_POOL_SIZE, usb.dma_memory_pool,
                             NX_USB_DMA_MEMORY_POOL_SIZE) != UX_SUCCESS) {
        goto init_failure;
    }

    if (ux_host_stack_initialize(host_change_function) != UX_SUCCESS) {
        goto init_failure;
    }

    if (ux_host_stack_class_register(_ux_system_host_class_hub_name, ux_host_class_hub_entry) != UX_SUCCESS) {
        goto init_failure;
    }

    if (ux_host_stack_hcd_register(_ux_system_host_hcd_ohci_name, ux_hcd_ohci_initialize, NX_USB_OHCI_MMIO_BASE, 0) !=
        UX_SUCCESS) {
        goto init_failure;
    }

    // Inject events for all root hub ports on startup in case OHCI hardware doesn't generate them for pre-connected devices
    for (UINT hcd_index = 0; hcd_index < _ux_system_host->ux_system_host_registered_hcd; hcd_index++) {
        if (_ux_system_host->ux_system_host_hcd_array[hcd_index].ux_hcd_io != NX_USB_OHCI_MMIO_BASE) {
            continue;
        }

        // Remember pointer to OHCI controller
        usb.hcd_ohci = (UX_HCD_OHCI *)_ux_system_host->ux_system_host_hcd_array[hcd_index].ux_hcd_controller_hardware;

        for (ULONG port_index = 0; port_index < UX_MAX_ROOTHUB_PORT; port_index++) {
            _ux_system_host->ux_system_host_hcd_array[hcd_index].ux_hcd_root_hub_signal[port_index]++;
        }
    }
    assert(usb.hcd_ohci);
    _ux_host_semaphore_put(&_ux_system_host->ux_system_host_enum_semaphore);

    return usb.count;

init_failure:
    int count = nxUsbShutdown();
    assert(count == 0);
    return -1;
}

int nxUsbShutdown() {
    if (usb.count == 0 || --usb.count > 0) {
        return usb.count;
    }

    assert(usb.count == 0);

    ux_host_stack_hcd_unregister(_ux_system_host_hcd_ohci_name, NX_USB_OHCI_MMIO_BASE, 0);
    ux_host_stack_class_unregister(ux_host_class_hub_entry);
    ux_host_stack_uninitialize();
    ux_system_uninitialize();

    // Clean up OHCI IRQ thread
    KeSetEvent(&usb.evt, IO_KEYBOARD_INCREMENT, FALSE);
    WaitForSingleObject(usb.irq_thread, INFINITE);
    CloseHandle(usb.irq_thread);

    // Clean up user registered callbacks
    PLIST_ENTRY entry = usb.change_callback_head.Flink;
    while (entry != &usb.change_callback_head) {
        nx_usb_change_callback_t *callback = CONTAINING_RECORD(entry, nx_usb_change_callback_t, entry);
        free(callback);
        entry = entry->Flink;
    }

    // Clean up memory pools
    if (usb.regular_memory_pool) {
        free(usb.regular_memory_pool);
    }
    if (usb.dma_memory_pool) {
        MmFreeContiguousMemory(usb.dma_memory_pool);
    }

    memset(&usb, 0, sizeof(usb));
    return usb.count;
}

int nxUsbRegisterEventCallback(UINT (*ux_system_host_change_function)(ULONG, UX_HOST_CLASS *, VOID *)) {
    if (ux_system_host_change_function == NULL) {
        return -1;
    }

    nx_usb_change_callback_t *callback = malloc(sizeof(nx_usb_change_callback_t));
    if (callback == NULL) {
        return -1;
    }

    callback->ux_system_host_change_function = ux_system_host_change_function;
    InsertTailList(&usb.change_callback_head, &callback->entry);
    return 0;
}

// Utility functions from USBX
VOID *_ux_utility_physical_address(VOID *virtual_address)
{
    return (virtual_address) ? ((VOID *)MmGetPhysicalAddress(virtual_address)) : UX_NULL;
}

VOID *_ux_utility_virtual_address(VOID *physical_address)
{
    return (physical_address) ? (VOID *)((uintptr_t)physical_address | 0x80000000) : UX_NULL;
}