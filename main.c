#include <hal/debug.h>
#include <hal/video.h>
#include <windows.h>

#include "ux_api.h"
#include "ux_system.h"
#include "ux_utility.h"
#include "ux_host_class_hub.h"
#include "ux_hcd_ohci.h"

typedef struct nx_usb_data
{
    int count;
    KINTERRUPT irq;
    KDPC dpc;
    KEVENT evt;
    HANDLE irq_thread;
    HANDLE task_thread;
    uintptr_t vtop;
} nx_usb_data_t;
static nx_usb_data_t usb = {0};

// ux_hcd_ohci_interrupt_handler
static BOOLEAN NTAPI isr(PKINTERRUPT Interrupt, PVOID ServiceContext)
{
    nx_usb_data_t *nx_usb_data = &usb;
    // FIXME, disable ohci IRQ somehow, and defer to DPC
    // Normally disabled by this: _ux_hcd_ohci_register_write(hcd_ohci, OHCI_HC_INTERRUPT_DISABLE, OHCI_HC_INT_MIE);
    ux_hcd_ohci_interrupt_handler();

    return TRUE;
    KeInsertQueueDpc(&nx_usb_data->dpc, NULL, NULL);
    return TRUE;
}

static void NTAPI dpc(PKDPC Dpc, PVOID DeferredContext, PVOID arg1, PVOID arg2)
{
    nx_usb_data_t *nx_usb_data = &usb;
    KeSetEvent(&nx_usb_data->evt, IO_KEYBOARD_INCREMENT, FALSE);
    return;
}

static DWORD WINAPI irq_process(LPVOID lpThreadParameter)
{
    nx_usb_data_t *nx_usb_data = lpThreadParameter;
    while (1)
    {
        KeWaitForSingleObject(&nx_usb_data->evt, Executive, KernelMode, FALSE, NULL);
        ux_hcd_ohci_interrupt_handler();
    }
    return 0;
}

UINT host_change_function(ULONG change_code, UX_HOST_CLASS *ux_class, VOID *param)
{
    switch (change_code)
    {
    case UX_DEVICE_INSERTION:
        debugPrint("USB device inserted\n");
        break;
    case UX_DEVICE_REMOVAL:
    case UX_DEVICE_DISCONNECTION:
        debugPrint("USB device removed\n");
        break;
    case UX_DEVICE_CONNECTION:
        UX_DEVICE *dev = param;
        debugPrint("USB device connected 0x%04x 0x%04x\n", dev->ux_device_descriptor.idVendor, dev->ux_device_descriptor.idProduct);
        break;
    default:
        debugPrint("Unknown USB device change %02lx\n", change_code);
        break;
    }
    return UX_SUCCESS;
}

int main(void)
{
    ULONG irqc, vector;
    KIRQL irql;

    XVideoSetMode(1280, 720, 16, REFRESH_DEFAULT);

    irqc = 1; // Or 9 for other OHCI controller
    vector = HalGetInterruptVector(irqc, &irql);
    KeInitializeInterrupt(&usb.irq, &isr, &usb, vector, irql, LevelSensitive, FALSE);
    KeInitializeDpc(&usb.dpc, dpc, &usb);
    KeInitializeEvent(&usb.evt, SynchronizationEvent, FALSE);
    usb.irq_thread = CreateThread(NULL, 0, irq_process, &usb, 0, NULL);
    KeConnectInterrupt(&usb.irq);

    // FIXME; work out a good number for memory pool sizes. Do we need cache safe really?
    static uint8_t ux_regular_memory_pool[0x20000];
    uint8_t *cache_safe_memory_pool = MmAllocateContiguousMemory(0x20000);

    debugPrint("%s\n", _ux_version_id);

    UX_ASSERT(ux_system_initialize(ux_regular_memory_pool, sizeof(ux_regular_memory_pool), cache_safe_memory_pool, 32768 * 2) == UX_SUCCESS);

    UX_ASSERT(ux_host_stack_initialize(host_change_function) == UX_SUCCESS);

    UX_ASSERT(ux_host_stack_class_register(_ux_system_host_class_hub_name, ux_host_class_hub_entry) == UX_SUCCESS);

    UX_ASSERT(ux_host_stack_hcd_register(_ux_system_host_hcd_ohci_name, ux_hcd_ohci_initialize, 0xFED00000, 0) == UX_SUCCESS);

    // Inject events for all root hub ports on startup incase the OHCI hardware doesn't generate them
    for (UINT hcd_index = 0; hcd_index < _ux_system_host->ux_system_host_registered_hcd; hcd_index++)
    {
        for (ULONG port_index = 0; port_index < UX_MAX_ROOTHUB_PORT; port_index++)
        {
            _ux_system_host->ux_system_host_hcd_array[hcd_index].ux_hcd_root_hub_signal[port_index]++;
        }
    }
    _ux_host_semaphore_put(&_ux_system_host->ux_system_host_enum_semaphore);

    while (1)
    {
        Sleep(1000);
    }

    return 0;
}
