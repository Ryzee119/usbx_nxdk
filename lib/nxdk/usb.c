// SPDX-License-Identifier: MIT

// SPDX-FileCopyrightText: 2024 Ryan Wendland

#include <stdlib.h>
#include <assert.h>
#include <nxdk/usb.h>
#include <windows.h>

#include "usb.h"
#include "ux_api.h"
#include "ux_host_stack.h"
#include "ux_host_class_hub.h"
#include "ux_hcd_ohci.h"

typedef struct nx_usb_change_callback {
    void (*ux_system_host_change_function)(ULONG, UX_HOST_CLASS *, VOID *);
    LIST_ENTRY entry;
} nx_usb_change_callback_t;

typedef struct nx_usb_data {
    UX_HCD_OHCI *hcd_ohci;
    KINTERRUPT irq;
    KDPC dpc;
    KEVENT evt;
    HANDLE irq_thread;
    HANDLE task_thread;
    uint32_t ref_count;
    uint8_t *regular_memory_pool;
    uint8_t *dma_memory_pool;
    LIST_ENTRY change_callback_head;
} nx_usb_data_t;
static nx_usb_data_t usb = {0};

static BOOLEAN NTAPI irq(PKINTERRUPT Interrupt, PVOID ServiceContext) {
    _ux_hcd_ohci_register_write(usb.hcd_ohci, OHCI_HC_INTERRUPT_DISABLE, OHCI_HC_INT_MIE);
    KeInsertQueueDpc(&usb.dpc, NULL, NULL);
    return TRUE;
}

static void NTAPI dpc(PKDPC Dpc, PVOID DeferredContext, PVOID arg1, PVOID arg2) {
    KeSetEvent(&usb.evt, IO_KEYBOARD_INCREMENT, FALSE);
    return;
}

static DWORD WINAPI isr(LPVOID lpThreadParameter) {
    while (1) {
        KeWaitForSingleObject(&usb.evt, Executive, KernelMode, FALSE, NULL);
        if (usb.ref_count == 0) {
            break;
        }
        ux_hcd_ohci_interrupt_handler();
        _ux_hcd_ohci_register_write(usb.hcd_ohci, OHCI_HC_INTERRUPT_ENABLE, OHCI_HC_INT_MIE);
    }
    return 0;
}

// See https://github.com/eclipse-threadx/rtos-docs/blob/main/rtos-docs/usbx/usbx-host-stack-4.md#input-parameter
static UINT host_change_function(ULONG change_code, UX_HOST_CLASS *ux_class, VOID *param) {
    PLIST_ENTRY entry = usb.change_callback_head.Flink;
    while (entry != &usb.change_callback_head) {
        nx_usb_change_callback_t *callback = CONTAINING_RECORD(entry, nx_usb_change_callback_t, entry);
        callback->ux_system_host_change_function(change_code, ux_class, param);
        entry = entry->Flink;
    }
    return UX_SUCCESS;
}

int nxUsbInit() {
    if (++usb.ref_count > 1) {
        return usb.ref_count;
    }
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

    InitializeListHead(&usb.change_callback_head);

    KIRQL irql;
    ULONG vector = HalGetInterruptVector((ULONG)NX_USB_OHCI_IRQ_CHANNEL, &irql);
    KeInitializeInterrupt(&usb.irq, &irq, &usb, vector, irql, LevelSensitive, FALSE);
    KeInitializeDpc(&usb.dpc, dpc, &usb);
    KeInitializeEvent(&usb.evt, SynchronizationEvent, FALSE);

    // Inject events for all root hub ports on startup in case OHCI hardware doesn't generate them for pre-connected
    // devices
    for (UINT hcd_index = 0; hcd_index < _ux_system_host->ux_system_host_registered_hcd; hcd_index++) {
        if (_ux_system_host->ux_system_host_hcd_array[hcd_index].ux_hcd_io != NX_USB_OHCI_MMIO_BASE) {
            continue;
        }

        for (ULONG port_index = 0; port_index < UX_MAX_ROOTHUB_PORT; port_index++) {
            _ux_system_host->ux_system_host_hcd_array[hcd_index].ux_hcd_root_hub_signal[port_index]++;
        }

        // While we are here, remember pointer to the OHCI controller
        usb.hcd_ohci = (UX_HCD_OHCI *)_ux_system_host->ux_system_host_hcd_array[hcd_index].ux_hcd_controller_hardware;

        break;
    }
    assert(usb.hcd_ohci);
    _ux_host_semaphore_put(&_ux_system_host->ux_system_host_enum_semaphore);

    KeConnectInterrupt(&usb.irq);
    usb.irq_thread = CreateThread(NULL, 0, isr, NULL, 0, NULL); // FIXME Use KeThread..

    return usb.ref_count;

init_failure:
    int ref_count = nxUsbShutdown();
    assert(ref_count == 0);
    return -1;
}

int nxUsbShutdown() {
    if (usb.ref_count == 0 || --usb.ref_count > 0) {
        return usb.ref_count;
    }

    assert(usb.ref_count == 0);

    ux_host_stack_hcd_unregister(_ux_system_host_hcd_ohci_name, NX_USB_OHCI_MMIO_BASE, 0);
    ux_host_stack_class_unregister(ux_host_class_hub_entry);
    ux_host_stack_uninitialize();
    ux_system_uninitialize();

    // Clean up OHCI IRQ thread
    if (usb.irq_thread) {
        KeSetEvent(&usb.evt, IO_KEYBOARD_INCREMENT, FALSE);
        WaitForSingleObject(usb.irq_thread, INFINITE);
        CloseHandle(usb.irq_thread);
    }

    KeDisconnectInterrupt(&usb.irq);

    // Clean up user registered change callbacks
    PLIST_ENTRY entry = usb.change_callback_head.Flink;
    while (entry != &usb.change_callback_head) {
        nx_usb_change_callback_t *callback = CONTAINING_RECORD(entry, nx_usb_change_callback_t, entry);
        free(callback);
        entry = entry->Flink;
    }

    // Clean up memory pools
    free(usb.regular_memory_pool);
    MmFreeContiguousMemory(usb.dma_memory_pool);

    memset(&usb, 0, sizeof(usb));
    return usb.ref_count;
}

int nxUsbRegisterChangeCallback(void (*ux_system_host_change_function)(ULONG, UX_HOST_CLASS *, VOID *)) {
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

static UINT nxusb_class_entry_function(UX_HOST_CLASS_COMMAND *command) {
    switch (command->ux_host_class_command_request) {
        case UX_HOST_CLASS_COMMAND_DEACTIVATE:
            nx_usb_device_t *nx_device = (nx_usb_device_t *)command->ux_host_class_command_instance;

            assert(nx_device->ux_device);
            UX_DEVICE *ux_device = nx_device->ux_device;

            // Prevent any more transfers occuring on this device
            EnterCriticalSection(&nx_device->lock);
            nx_device->ux_device = NULL;
            LeaveCriticalSection(&nx_device->lock);

            // Send abort to any active control transfers
            ux_host_stack_endpoint_transfer_abort(&ux_device->ux_device_control_endpoint);

            // Send abort to any active pipe transfers
            UX_CONFIGURATION *ux_configuration = ux_device->ux_device_current_configuration;
            if (ux_configuration) {
                UX_INTERFACE *ux_interface = ux_configuration->ux_configuration_first_interface;
                while (ux_interface != NULL) {
                    UX_ENDPOINT *ux_endpoint = ux_interface->ux_interface_first_endpoint;
                    while (ux_endpoint != NULL) {
                        ux_host_stack_endpoint_transfer_abort(ux_endpoint);
                        ux_endpoint = ux_endpoint->ux_endpoint_next_endpoint;
                    }
                    ux_interface = ux_interface->ux_interface_next_interface;
                }
            }     
            
            return (UX_SUCCESS);
        default:
            return (UX_FUNCTION_NOT_SUPPORTED);
    }
}

static UX_HOST_CLASS nxusb_host_class = {
    .ux_host_class_name = "nxusb",
    .ux_host_class_entry_function = nxusb_class_entry_function,
    .ux_host_class_first_instance = NULL,
    .ux_host_class_client = NULL,
    .ux_host_class_media = NULL,
    .ux_host_class_ext = NULL,
};

int nxUsbDeviceClaim(match_device_id_t *device_ids, match_device_class_t *device_class,
                     match_interface_class_t *interface_class, nx_usb_device_t *nx_device) {
    UX_DEVICE *ux_device = NULL;
    UX_CONFIGURATION *ux_configuration = NULL;
    UX_INTERFACE *ux_interface = NULL;
    UINT device_index = 0;
    ULONG interface_index = 0;

    // Have you run nxUsbInit()?
    if (_ux_system == NULL || _ux_system_host == NULL || _ux_system_host->ux_system_host_device_array == NULL) {
        return -1;
    }

    if (nx_device == NULL) {
        return -1;
    }

    // We dont want USBX changing stuff on us if user hotplugs so grab ux system mutex.
    _ux_system_mutex_on(&_ux_system->ux_system_mutex);
    for (device_index = 0; device_index < UX_MAX_DEVICES; device_index++) {
        ux_device = &_ux_system_host->ux_system_host_device_array[device_index];
        if (ux_device->ux_device_handle != (ULONG)ux_device) {
            continue;
        }
        if (ux_device->ux_device_state == UX_DEVICE_REMOVED) {
            continue;
        }
        if (ux_device->ux_device_class_instance != (ULONG)NULL) {
            continue;
        }

        // We check each device for a match against ids or classes
        if (device_ids) {
            if (ux_device->ux_device_descriptor.idVendor != device_ids->vid ||
                ux_device->ux_device_descriptor.idProduct != device_ids->pid) {
                continue;
            } else {
                break;
            }
        }

        else if (device_class) {
            if (ux_device->ux_device_descriptor.bDeviceClass != device_class->bDeviceClass ||
                ux_device->ux_device_descriptor.bDeviceSubClass != device_class->bDeviceSubClass ||
                ux_device->ux_device_descriptor.bDeviceProtocol != device_class->bDeviceProtocol) {
                continue;
            } else {
                break;
            }
        }

        else if (interface_class) {
            if (ux_host_stack_device_configuration_get(ux_device, 0, &ux_configuration) != UX_SUCCESS) {
                continue;
            }

            ux_interface = ux_configuration->ux_configuration_first_interface;
            while (ux_interface != NULL) {
                if (ux_interface->ux_interface_descriptor.bInterfaceClass == interface_class->bInterfaceClass &&
                    ux_interface->ux_interface_descriptor.bInterfaceSubClass == interface_class->bInterfaceSubClass &&
                    ux_interface->ux_interface_descriptor.bInterfaceProtocol == interface_class->bInterfaceProtocol) {
                    interface_index = ux_interface->ux_interface_descriptor.bInterfaceNumber;
                    break;
                }
                ux_interface = ux_interface->ux_interface_next_interface;
            }
            if (ux_interface != NULL) {
                break;
            }
        }
        ux_device = NULL;
    }

    if (device_index == UX_MAX_DEVICES) {
        ux_device = NULL;
    }

    if (ux_device) {
        ux_device->ux_device_class = &nxusb_host_class;
        ux_device->ux_device_class_instance = nx_device;
        nx_device->ux_device = ux_device;
        InitializeCriticalSection(&nx_device->lock);
    }

    _ux_system_mutex_off(&_ux_system->ux_system_mutex);

    if (ux_device == NULL) {
        return -1;
    }

    // On interface match, return the interface number it was located at.
    if (interface_class && ux_interface != NULL) {
        return interface_index;
    } else {
        return 0;
    }
}

int nxUsbDeviceRelease(nx_usb_device_t *nx_device) {
    EnterCriticalSection(&nx_device->lock);

    _ux_system_mutex_on(&_ux_system->ux_system_mutex);
    if (nx_device->ux_device) {
        UX_INTERRUPT_SAVE_AREA
        UX_DISABLE
        nx_device->ux_device->ux_device_class_instance = NULL;
        nx_device->ux_device->ux_device_class = NULL;
        UX_RESTORE
        nx_device->ux_device = NULL;
    }
    _ux_system_mutex_off(&_ux_system->ux_system_mutex);

    LeaveCriticalSection(&nx_device->lock);
    DeleteCriticalSection(&nx_device->lock);
    return 0;
}

// Utility functions from USBX
VOID *_ux_utility_physical_address(VOID *virtual_address) {
    return (virtual_address) ? ((VOID *)MmGetPhysicalAddress(virtual_address)) : NULL;
}

VOID *_ux_utility_virtual_address(VOID *physical_address) {
    return (physical_address) ? (VOID *)((uintptr_t)physical_address | 0x80000000) : NULL;
}
